#pragma once

// Drill 4 - Job system kata
// 	Job system kata — lock-free ring + work-stealing queue, Catch2 tests ensure FIFO & no duplicates

/* 
Spec: Implement a global lock-free ring buffer for multi-producer task submission and a per-worker work-stealing deque so idle threads can pull work from their neighbors without locks.

Tests: 
  • Single-thread enqueue/dequeue fidelity  
  • Multi-thread FIFO ordering under contention  
  • No tasks lost or executed more than once  
  • Steal operations succeed and preserve overall ordering in stress tests

Refactor: 
  • Add batching of small tasks for throughput  
  • Introduce dynamic resizing of the ring buffer  
  • Layer in priority or affinity hints for advanced scheduling

Why: Demonstrates mastery of atomic primitives, memory ordering, and thread coordination—key skills for engine bring-up on new platforms where every cycle and cache line counts.
*/

#include <unique_buffer.h>
#include <atomic>
#include <functional>

template <typename T>
class JobQueue {
  public:
    JobQueue<T>() : 
        m_capacity(0), m_entries(0),
        m_read(0), m_write(0)
        {}

    JobQueue<T>(const size_t capacity) : 
          m_capacity(capacity), m_entries(capacity),
          m_read(0), m_write(0)
        {
        }
    
    JobQueue<T>(JobQueue<T>&& src)
      : m_entries(std::move(src.m_entries)),
        m_capacity(src.m_capacity),
        m_read(src.m_read.load(std::memory_order_relaxed)),
        m_write(src.m_write.load(std::memory_order_relaxed)) {
        // mark the moved from object as empty but valid
        src.m_capacity = 0;
    }

    JobQueue<T>& operator=(JobQueue<T>&& src) {
      if (this != &src) {
        m_entries = std::move(src.m_entries);
        m_capacity = src.m_capacity;
        m_read = src.m_read.load(std::memory_order_relaxed);
        m_write = src.m_write.load(std::memory_order_relaxed);
        src.m_capacity = 0;
      }
      return *this;
    }

    JobQueue<T> operator=(const JobQueue<T>& copy) = delete;
    JobQueue<T>(const JobQueue<T>& copy) = delete;
    
  private:
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_write;
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_read;

    UniqueBuffer<T> m_entries;
    size_t m_capacity;

  public:
    bool push(T item) 
    {
      size_t tail, head;
      while (true)
      {
          tail = m_write.load(std::memory_order_relaxed);
          head = m_read.load(std::memory_order_acquire);            // pairs with pop

          if (tail - head >= m_capacity) return false;               // full

          m_entries[tail % m_capacity] = item;                // fill

          if (m_write.compare_exchange_weak(tail, tail + 1,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
          {
            return true;
          }
      }
    }

    bool pop(T& out) {
      size_t head, tail;
      while (true)
      {
          head = m_read.load(std::memory_order_relaxed);
          tail = m_write.load(std::memory_order_acquire);            // pairs with push

          if (tail == head) return false;                            // empty

          if (m_read.compare_exchange_weak(head, head + 1,
                                          std::memory_order_release,
                                          std::memory_order_relaxed))
          {
              out = std::move(m_entries[head % m_capacity]);
              return true;
              break;                                                 // claimed slot
          }
      }

    }

    bool steal(JobQueue<T>& victim, T& out) {
      size_t head, tail;
      while (true)
      {
          head = victim.m_read.load(std::memory_order_relaxed);
          tail = victim.m_write.load(std::memory_order_acquire);            // pairs with push

          if (tail == head) return false;                            // empty

          if (victim.m_read.compare_exchange_weak(head, head + 1,
                                          std::memory_order_release,
                                          std::memory_order_relaxed))
          {
              out = std::move(victim.m_entries[head % victim.m_capacity]);
              return true;
              break;                                                 // claimed slot
          }
      }
    }

};