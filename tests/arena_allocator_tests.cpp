#include <catch2/catch_test_macros.hpp>
#include <cstddef> // For std::byte, size_t
#include <cstdint> // For uintptr_t
#include <vector>  // For testing allocations of multiple objects

#include <arena_allocator.h>

// Helper to check alignment
bool is_aligned(void* ptr, size_t alignment) {
    if (alignment == 0) return false; // Alignment cannot be zero
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

// Helper struct for testing with specific alignment
struct alignas(8) AlignedStruct8 {
    uint64_t a;
    char c;
};

struct alignas(16) AlignedStruct16 {
    uint64_t a, b;
    char c;
};

struct SimpleStruct {
    int x;
    int y;
};


TEST_CASE("ArenaAllocator: Basic Construction and Reset", "[arena_allocator]") {
    ArenaAllocator arena(1024); // 1KB arena

    // After construction, an allocation should succeed
    void* p1 = arena.allocate(10, 1);
    REQUIRE(p1 != nullptr);

    arena.reset();

    // After reset, a similar allocation should yield a pointer at the same (or similar initial) address
    // (Assuming reset brings it back to the very beginning)
    void* p2 = arena.allocate(10, 1);
    REQUIRE(p2 != nullptr);
    // Depending on how you manage the start, p2 might be equal to p1 if no internal state changes.
    // For simplicity, let's assume reset truly resets to initial state.
    // More robustly, we could just check it's non-null and within expected bounds.
    
    REQUIRE( p1 == p2 );
}

TEST_CASE("ArenaAllocator: Simple Sequential Allocations", "[arena_allocator]") {
    ArenaAllocator arena(1024);

    void* p1 = arena.allocate(100, 1);
    REQUIRE(p1 != nullptr);

    void* p2 = arena.allocate(200, 1);
    REQUIRE(p2 != nullptr);
    REQUIRE(reinterpret_cast<std::byte*>(p2) >= reinterpret_cast<std::byte*>(p1) + 100); // p2 comes after p1

    void* p3 = arena.allocate(300, 1);
    REQUIRE(p3 != nullptr);
    REQUIRE(reinterpret_cast<std::byte*>(p3) >= reinterpret_cast<std::byte*>(p2) + 200); // p3 comes after p2
}

TEST_CASE("ArenaAllocator: Allocation Failure When Full", "[arena_allocator]") {
    ArenaAllocator arena(100); // Small arena

    void* p1 = arena.allocate(70, 1);
    REQUIRE(p1 != nullptr);

    void* p2 = arena.allocate(30, 1);
    REQUIRE(p2 != nullptr); // Should exactly fit

    void* p3 = arena.allocate(1, 1); // This should fail
    REQUIRE(p3 == nullptr);

    // Reset and try something that would have fit before but now also fails
    arena.reset();
    void* p4 = arena.allocate(101, 1);
    REQUIRE(p4 == nullptr);
}


TEST_CASE("ArenaAllocator: Alignment Handling", "[arena_allocator]") {
    ArenaAllocator arena(1024);

    SECTION("Allocate with alignment 1 (no specific alignment)") {
        void* p = arena.allocate(10, 1);
        REQUIRE(p != nullptr);
        // No specific alignment check needed beyond being non-null
    }

    SECTION("Allocate with alignment 8") {
        // Allocate a small object first to potentially misalign the next free pointer
        arena.allocate(3, 1); 

        void* p8 = arena.allocate(sizeof(AlignedStruct8), alignof(AlignedStruct8));
        REQUIRE(p8 != nullptr);
        REQUIRE(is_aligned(p8, alignof(AlignedStruct8)));
        REQUIRE(is_aligned(p8, 8)); // alignof(AlignedStruct8) should be 8
    }

    SECTION("Allocate with alignment 16") {
        arena.allocate(7, 1); // Misalign

        void* p16 = arena.allocate(sizeof(AlignedStruct16), alignof(AlignedStruct16));
        REQUIRE(p16 != nullptr);
        REQUIRE(is_aligned(p16, alignof(AlignedStruct16)));
        REQUIRE(is_aligned(p16, 16));

        // Allocate another aligned struct
        void* p16_2 = arena.allocate(sizeof(AlignedStruct16), alignof(AlignedStruct16));
        REQUIRE(p16_2 != nullptr);
        REQUIRE(is_aligned(p16_2, alignof(AlignedStruct16)));
        // Check it's after the previous one
        REQUIRE(reinterpret_cast<std::byte*>(p16_2) >= reinterpret_cast<std::byte*>(p16) + sizeof(AlignedStruct16));
    }

    SECTION("Multiple varied alignments") {
        arena.allocate(1, 1); // Potentially misalign start for next allocation

        void* s8_ptr = arena.allocate(sizeof(AlignedStruct8), alignof(AlignedStruct8));
        REQUIRE(s8_ptr != nullptr);
        REQUIRE(is_aligned(s8_ptr, alignof(AlignedStruct8)));

        // No specific misalignment needed here as s8_ptr itself might misalign for s16_ptr
        void* s16_ptr = arena.allocate(sizeof(AlignedStruct16), alignof(AlignedStruct16));
        REQUIRE(s16_ptr != nullptr);
        REQUIRE(is_aligned(s16_ptr, alignof(AlignedStruct16)));
        REQUIRE(reinterpret_cast<std::byte*>(s16_ptr) >= reinterpret_cast<std::byte*>(s8_ptr) + sizeof(AlignedStruct8));

        void* simple_ptr = arena.allocate(sizeof(SimpleStruct), alignof(SimpleStruct));
        REQUIRE(simple_ptr != nullptr);
        REQUIRE(is_aligned(simple_ptr, alignof(SimpleStruct))); // alignof(SimpleStruct) is likely 4
        REQUIRE(reinterpret_cast<std::byte*>(simple_ptr) >= reinterpret_cast<std::byte*>(s16_ptr) + sizeof(AlignedStruct16));
    }
}

TEST_CASE("ArenaAllocator: Allocation Failure Due to Alignment Padding", "[arena_allocator]") {
    ArenaAllocator arena(100);

    // Allocate 95 bytes. Current pointer will be at offset 95.
    arena.allocate(95, 1); 
    
    // Request 4 bytes with alignment 8.
    // To align, it needs to go to offset 96 (1 byte padding). Total needed: 1 (padding) + 4 (size) = 5 bytes.
    // Remaining space is 100 - 95 = 5 bytes. This should fit.
    void* p1 = arena.allocate(4, 8);
    REQUIRE(p1 != nullptr);
    REQUIRE(is_aligned(p1, 8));

    // Arena is now full (95 used for first, 1 for padding, 4 for second = 100 bytes total from start)
    // Reset for next test
    arena.reset();
    arena.allocate(95, 1);

    // Request 5 bytes with alignment 8.
    // To align, it needs to go to offset 96 (1 byte padding). Total needed: 1 (padding) + 5 (size) = 6 bytes.
    // Remaining space is 5 bytes. This should FAIL.
    void* p2 = arena.allocate(5, 8);
    REQUIRE(p2 == nullptr);
}

TEST_CASE("ArenaAllocator: Reset Empties Arena for Reuse", "[arena_allocator]") {
    ArenaAllocator arena(128);
    std::vector<void*> allocations;

    for(int i = 0; i < 10; ++i) {
        allocations.push_back(arena.allocate(10, 1));
        REQUIRE(allocations.back() != nullptr);
    }
    // Arena should be somewhat full (100 bytes used)
    REQUIRE(arena.allocate(30, 1) == nullptr); // Should fail or be near failing

    arena.reset();
    allocations.clear();

    // After reset, should be able to allocate significantly again
    for(int i = 0; i < 12; ++i) { // Try to allocate 120 bytes
        allocations.push_back(arena.allocate(10, 1));
        REQUIRE(allocations.back() != nullptr);
    }
    REQUIRE(arena.allocate(10, 1) == nullptr); // Should fail as 120/128 used
}

TEST_CASE("ArenaAllocator: Edge Cases", "[arena_allocator]") {
    ArenaAllocator arena(100);

    SECTION("Allocate zero bytes") {
        // What should happen? Some allocators return nullptr, some return a unique non-null pointer (if alignment allows).
        // Let's assume for now it should still be non-null if arena isn't full, and alignment is met.
        // The pointer itself might point to the current free ptr.
        // This behavior might need clarification based on typical use cases for zero-byte allocations.
        // For safety and typical arena usage (allocating actual objects), perhaps it's best it just works like any other alloc.
        void* p_zero = arena.allocate(0, 1);
        REQUIRE(p_zero != nullptr); 

        void* p_after_zero = arena.allocate(10, 1);
        REQUIRE(p_after_zero != nullptr);
        // p_after_zero should ideally be at the same location as p_zero if p_zero didn't advance the pointer.
        // Or, if allocate(0) advances by alignment padding, then p_after_zero is after that.
        // Let's define: allocate(0, align) should return an aligned pointer and consume no object space,
        // but may consume alignment padding. The next allocation would start from that aligned pointer.
        
        // Re-evaluating: A common behavior for allocate(0) is to return a valid, aligned pointer,
        // and the next allocation happens immediately after it (no "object_size" added).
        // Let's test that if we request 0 bytes, we get an aligned ptr, and the next alloc starts there.
        arena.reset();
        arena.allocate(3, 1); // Misalign to 3
        void* pz = arena.allocate(0, 8); // Request 0 bytes, 8-byte alignment
        REQUIRE(pz != nullptr);
        REQUIRE(is_aligned(pz, 8));
        void* p_next = arena.allocate(10, 1); // Next allocation should start at pz
        REQUIRE(p_next == pz); // Or p_next == reinterpret_cast<std::byte*>(pz)
    }
    
    SECTION("Alignment larger than object size") {
        void* p = arena.allocate(4, 16); // Allocate 4 bytes with 16-byte alignment
        REQUIRE(p != nullptr);
        REQUIRE(is_aligned(p, 16));
    }

    SECTION("Alignment is zero (invalid)") {
        // This is an invalid argument. How should the allocator react?
        // Throw an exception? Return nullptr? Assert in debug?
        // For now, let's assume it should return nullptr as a safe default.
        REQUIRE(arena.allocate(10, 0) == nullptr);
    }

    SECTION("Arena size exactly matches aligned allocation") {
        ArenaAllocator small_arena(16); // Arena of 16 bytes
        void* p = small_arena.allocate(8, 8); // Request 8 bytes, 8-byte aligned. Starts at 0. Uses 0-7.
        REQUIRE(p != nullptr);
        REQUIRE(is_aligned(p, 8));
        void* p2 = small_arena.allocate(8, 8); // Request 8 bytes, 8-byte aligned. Starts at 8. Uses 8-15.
        REQUIRE(p2 != nullptr);
        REQUIRE(is_aligned(p2, 8));
        REQUIRE(reinterpret_cast<std::byte*>(p2) == reinterpret_cast<std::byte*>(p) + 8);
        REQUIRE(small_arena.allocate(1,1) == nullptr); // Arena should be full
    }

    SECTION("Arena too small for even minimal alignment padding + smallest object") {
        ArenaAllocator tiny_arena(7); // Arena of 7 bytes
        // Try to allocate 1 byte with 8-byte alignment.
        // Needs to align to an 8-byte boundary. If start is 0, next 8-byte boundary is 0 or 8.
        // If it aligns to 0, needs 1 byte. Fits.
        // If it aligns to 8 (e.g. if current_ptr was 1 and it needed to jump to 8), it wouldn't fit.
        // The formula (ptr + align - 1) & ~(align - 1) will handle this.
        // If initial ptr is 0, (0 + 8 - 1) & ~7 = 7 & ~7 = 0.  So it would allocate at 0.
        void* p_tiny = tiny_arena.allocate(1, 8);
        REQUIRE(p_tiny != nullptr);
        REQUIRE(is_aligned(p_tiny, 8));
        REQUIRE(tiny_arena.allocate(7,1) == nullptr); // only 6 bytes left (0 used for p_tiny)
                                                     // wait, 1 byte used, 6 left. yes.
        
        ArenaAllocator tiniest_arena(3);
        void* p_tiniest = tiniest_arena.allocate(4,1); // Request 4 bytes from 3 byte arena
        REQUIRE(p_tiniest == nullptr);
    }
}

// TODO, if time allows
// Consider adding tests for:
// - What if total_size for ArenaAllocator is 0?
// - Allocating an object whose alignment requirement is not a power of two (though rare, how does your math handle it?)
//   (The bitwise trick `(ptr + align - 1) & ~(align - 1)` assumes power-of-two alignment)