#pragma once

#include <cstddef>
#include <memory>
#include <mutex>

#include <unique_buffer.h>

class ArenaAllocator
{
public:
    ArenaAllocator(size_t total_size)
        : m_buffer(total_size),
          m_offset(0),
          m_current(m_buffer.data()),
          m_start(reinterpret_cast<uintptr_t>(m_buffer.data())),
          m_end(reinterpret_cast<uintptr_t>(m_buffer.data() + total_size)) {}

    void* allocate(size_t object_size, size_t object_alignment) {
        std::scoped_lock lock(m_mutex);
        uintptr_t current_addr = reinterpret_cast<uintptr_t>(m_current);
        // power of 2 alignment - sourced from Gemini Pro
        uintptr_t aligned_addr = (current_addr + object_alignment - 1) & ~(object_alignment - 1);

        // bounds check - ensure the requested address won't allocate beyond our unique buffer
        if (aligned_addr + object_size > m_end) {
            return nullptr;
        }

        // move the arena pointer forward for next alloc & return requested addr
        m_current = reinterpret_cast<std::byte*>(aligned_addr + object_size);
        return reinterpret_cast<std::byte*>(aligned_addr);;
    }

    // void *allocate(size_t object_size, size_t object_alignment) {
    //     uintptr_t current_offset = m_offset.load(std::memory_order_relaxed);
    //     uintptr_t current_addr = m_start + current_offset;
    //     uintptr_t aligned_addr;

    //     do {
    //         aligned_addr = (current_addr + object_alignment - 1) & ~(object_alignment - 1);

    //         // bounds check - ensure the requested address won't allocate beyond our unique buffer
    //         if (aligned_addr + object_size > m_end) {
    //             return nullptr;
    //         }
    //     } while (!m_offset.compare_exchange_weak(current_offset, current_offset + 1));

    //     // move the arena pointer forward for next alloc & return requested addr
    //     m_current = reinterpret_cast<std::byte*>(aligned_addr + object_size);
    //     return reinterpret_cast<std::byte*>(aligned_addr);;
    // }

    void reset() {
        // todo: fix
        m_offset.store(0);
        m_current = reinterpret_cast<std::byte *>(m_start);
    }

private:
    UniqueBuffer<std::byte> m_buffer;
    std::atomic<uintptr_t> m_offset;
    std::byte *m_current;
    uintptr_t const m_start; // Pointer to the beginning of the buffer
    uintptr_t const m_end;   // Pointer to one-past-the-end of the buffer
    std::mutex m_mutex;
};