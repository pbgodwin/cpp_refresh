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
        m_capacity(0), m_entries(m_capacity), 
        m_read(0), m_write(0), m_count(0)
        {}

    JobQueue<T>(const size_t capacity) : 
          m_capacity(capacity), m_entries(capacity),
          m_read(0), m_write(0), m_count(0)
        {

        }
    
    JobQueue<T> operator=(const JobQueue<T>& copy) = delete;
    JobQueue<T>(const JobQueue<T>& copy) = delete;
    
  private:
    UniqueBuffer<T> m_entries;
    const size_t m_capacity; // 4
    std::atomic<size_t> m_count;
    std::atomic<size_t> m_read; // 0
    std::atomic<size_t> m_write; // 0 

    bool try_claim_entry(std::atomic<size_t>& position_to_advance, 
                        const std::atomic<size_t>& boundary_to_check,
                        size_t& claimed_position)
    {
      size_t current_offset = position_to_advance.load();
      size_t next_position = current_offset + 1;

      if (next_position == m_capacity) {
        next_position = 0;
      }

      while (!position_to_advance.compare_exchange_strong(current_offset, next_position, std::memory_order_release)) {
        current_offset = position_to_advance.load();
        next_position = current_offset + 1;

        if (next_position == m_capacity) {
          next_position = 0;
        }
      }

      claimed_position = current_offset;
      return true;
    }


  public:
    bool push(T item) 
    {
      size_t current_count = m_count.load();
      if (current_count == m_capacity) {
        return false;
      }

      size_t next_count = current_count + 1;

      size_t write_index;
      if (try_claim_entry(m_write, m_read, write_index)) {
        m_entries[write_index] = std::move(T(item));
        while (!m_count.compare_exchange_strong(current_count, next_count, std::memory_order_acquire)) {
          current_count = m_count.load();
          if (current_count == m_capacity) {
            // queue was filled in the meantime
            return false;
          }
          next_count = current_count + 1;
        }
        return true;
      }

      return false;
    }

    bool pop(T& item) {
      size_t current_count = m_count.load();
      if (current_count == 0) {
        return false;
      }
      
      size_t next_count = current_count - 1;

      size_t read_index;
      if (try_claim_entry(m_read, m_write, read_index)) {
        item = std::move(m_entries[read_index]);
        while (!m_count.compare_exchange_strong(current_count, next_count, std::memory_order_acquire)) {
          current_count = m_count.load();
          if (current_count == 0) {
            // the work was lost? this seems invalid...
            return false;
          }
          next_count = current_count - 1;
        }
        return true;
      }

      return false;
    }

    bool steal(JobQueue<T>& queue, T& item) {
      size_t current_count = queue.m_count.load();
      if (current_count == 0) {
        return false;
      }

      size_t next_count = current_count - 1;

      size_t read_index;
      if (try_claim_entry(queue.m_read, queue.m_write, read_index)) {
        item = std::move(queue.m_entries[read_index]);
        while (!queue.m_count.compare_exchange_strong(current_count, next_count, std::memory_order_acquire)) {
          current_count = queue.m_count.load();
          if (current_count == 0) {
            // the work was lost? this seems invalid...
            return false;
          }
          next_count = current_count - 1;
        }
        return true;
      }

      return false;
    }

};