
#include <cstddef>
#include <memory>

#include <unique_buffer.h>

class ArenaAllocator {
public:
    ArenaAllocator(size_t total_size) 
        : m_buffer(total_size),
          m_current(m_buffer.data()),
          m_start(m_buffer.data()),
          m_end(m_buffer.data() + total_size) {}

    void* allocate(size_t object_size, size_t object_alignment) {
        uintptr_t current_addr = reinterpret_cast<uintptr_t>(m_current);
        // power of 2 alignment - sourced from Gemini Pro
        uintptr_t aligned_addr = (current_addr + object_alignment - 1) & ~(object_alignment - 1);

        // bounds check - ensure the requested address won't allocate beyond our unique buffer
        if (aligned_addr + object_size > reinterpret_cast<uintptr_t>(m_end)) {
            return nullptr;
        }

        // move the arena pointer forward for next alloc & return requested addr
        m_current = reinterpret_cast<std::byte*>(aligned_addr + object_size);
        return reinterpret_cast<std::byte*>(aligned_addr);;
    }

    void reset() {
        m_current = m_start;
    }

private:
    UniqueBuffer<std::byte> m_buffer;
    std::byte* m_current;
    std::byte* const m_start; // Pointer to the beginning of the buffer
    std::byte* const m_end;   // Pointer to one-past-the-end of the buffer
};