// tests/small_vector_tests.cpp
#include <catch2/catch_test_macros.hpp>
#include <small_vector.h> // You will create this
#include <string>         // For testing with a type that has move/copy/dtor
#include <stdexcept>      // For std::out_of_range

// A simple struct to track constructions, destructions, copies, moves
struct Tracker {
    static int constructions;
    static int destructions;
    static int copies;
    static int moves;

    int id;

    Tracker(int i = 0) : id(i) { constructions++; }
    ~Tracker() { destructions++; }
    Tracker(const Tracker& other) : id(other.id) { copies++; }
    Tracker(Tracker&& other) noexcept : id(other.id) { moves++; other.id = -1; /* Mark as moved from */ }
    Tracker& operator=(const Tracker& other) { 
        if (this != &other) { id = other.id; copies++; }
        return *this;
    }
    Tracker& operator=(Tracker&& other) noexcept {
        if (this != &other) { id = other.id; moves++; other.id = -1; }
        return *this;
    }
    bool operator==(const Tracker& other) const { return id == other.id; }

    static void reset_counts() {
        constructions = 0;
        destructions = 0;
        copies = 0;
        moves = 0;
    }
};
int Tracker::constructions = 0;
int Tracker::destructions = 0;
int Tracker::copies = 0;
int Tracker::moves = 0;


TEST_CASE("SmallVector: Basic Construction (int, N=3)", "[small_vector]") {
    SECTION("Default constructor") {
        SmallVector<int, 3> sv;
        REQUIRE(sv.empty());
        REQUIRE(sv.size() == 0);
        REQUIRE(sv.capacity() == 3); // Initial capacity is N
        REQUIRE(sv.on_stack());      // Should be on stack initially
    }

    SECTION("Constructor with size (all on stack)") {
        SmallVector<int, 3> sv(2); // Create 2 default-constructed ints
        REQUIRE_FALSE(sv.empty());
        REQUIRE(sv.size() == 2);
        REQUIRE(sv.capacity() == 3);
        REQUIRE(sv.on_stack());
        // REQUIRE(sv[0] == 0); // Default int value
        // REQUIRE(sv[1] == 0);
    }
    
    SECTION("Constructor with size and value (all on stack)") {
        SmallVector<int, 3> sv(2, 100);
        REQUIRE(sv.size() == 2);
        REQUIRE(sv.capacity() == 3);
        REQUIRE(sv.on_stack());
        REQUIRE(sv[0] == 100);
        REQUIRE(sv[1] == 100);
    }

    SECTION("Constructor with size (triggers heap allocation)") {
        SmallVector<int, 3> sv(5); // 5 elements, N=3, should go to heap
        REQUIRE_FALSE(sv.empty());
        REQUIRE(sv.size() == 5);
        REQUIRE(sv.capacity() >= 5); // Capacity might be more than 5 due to growth strategy
        REQUIRE_FALSE(sv.on_stack());
        // REQUIRE(sv[4] == 0); // Default int value
    }

    SECTION("Initializer list constructor (on stack)") {
        SmallVector<int, 5> sv = {10, 20, 30};
        REQUIRE(sv.size() == 3);
        REQUIRE(sv.capacity() == 5);
        REQUIRE(sv.on_stack());
        REQUIRE(sv[0] == 10);
        REQUIRE(sv[2] == 30);
    }

    SECTION("Initializer list constructor (triggers heap allocation)") {
        SmallVector<int, 2> sv = {10, 20, 30, 40};
        REQUIRE(sv.size() == 4);
        REQUIRE(sv.capacity() >= 4);
        REQUIRE_FALSE(sv.on_stack());
        REQUIRE(sv[0] == 10);
        REQUIRE(sv[3] == 40);
    }
}

TEST_CASE("SmallVector: Push_back operations (int, N=2)", "[small_vector]") {
    SmallVector<int, 2> sv;

    // Push 1: on stack
    sv.push_back(10);
    REQUIRE(sv.size() == 1);
    REQUIRE(sv.capacity() == 2);
    REQUIRE(sv.on_stack());
    REQUIRE(sv[0] == 10);

    // Push 2: on stack, fills stack capacity
    sv.push_back(20);
    REQUIRE(sv.size() == 2);
    REQUIRE(sv.capacity() == 2);
    REQUIRE(sv.on_stack());
    REQUIRE(sv[0] == 10);
    REQUIRE(sv[1] == 20);

    // Push 3: triggers heap allocation
    sv.push_back(30);
    REQUIRE(sv.size() == 3);
    REQUIRE(sv.capacity() >= 3); // Might be 2*N or N + N/2 etc.
    REQUIRE_FALSE(sv.on_stack());
    REQUIRE(sv[0] == 10); // Check if elements were moved correctly
    REQUIRE(sv[1] == 20);
    REQUIRE(sv[2] == 30);

    // Push 4: on heap
    sv.push_back(40);
    REQUIRE(sv.size() == 4);
    REQUIRE(sv.capacity() >= 4);
    REQUIRE_FALSE(sv.on_stack());
    REQUIRE(sv[3] == 40);
}

TEST_CASE("SmallVector: Pop_back operations (int, N=2)", "[small_vector]") {
    SmallVector<int, 2> sv = {1, 2, 3, 4, 5}; // Starts on heap
    REQUIRE(sv.size() == 5);
    REQUIRE_FALSE(sv.on_stack());

    sv.pop_back(); // size 4
    REQUIRE(sv.size() == 4);
    REQUIRE(sv[3] == 4);

    sv.pop_back(); // size 3
    REQUIRE(sv.size() == 3);
    REQUIRE(sv[2] == 3);

    // Optional: Consider if SmallVector should shrink back to stack if size becomes <= N.
    // For this test, let's assume it stays on heap once it goes there, unless explicitly shrunk.
    // (Simpler to implement initially)
    sv.pop_back(); // size 2
    REQUIRE(sv.size() == 2);
    REQUIRE(sv[1] == 2);
    REQUIRE_FALSE(sv.on_stack()); // Assuming no shrink-to-stack on pop_back

    sv.pop_back(); // size 1
    REQUIRE(sv.size() == 1);
    REQUIRE(sv[0] == 1);

    sv.pop_back(); // size 0
    REQUIRE(sv.size() == 0);
    REQUIRE(sv.empty());

    REQUIRE_THROWS_AS(sv.pop_back(), std::out_of_range); // Pop from empty
}

TEST_CASE("SmallVector: Pop_back with Tracker (N=1)", "[small_vector]") {
    Tracker::reset_counts();
    SmallVector<Tracker, 1> sv;
    sv.push_back(Tracker(10)); // On stack
    sv.push_back(Tracker(20)); // Now on heap {10, 20}
    REQUIRE(sv.size() == 2);
    REQUIRE(Tracker::constructions == 2); 
    REQUIRE(Tracker::destructions == 3  );  // Assuming 2 original stack items destructed on move to heap
    Tracker::reset_counts(); // Reset for pop_back check

    sv.pop_back(); // Should pop Tracker(20)
    REQUIRE(sv.size() == 1);
    REQUIRE(Tracker::destructions == 1); // Destructor for Tracker(20) should have been called
    REQUIRE(sv[0].id == 10); // Check remaining element if operator[] works
    Tracker::reset_counts();

    sv.pop_back(); // Should pop Tracker(10)
    REQUIRE(sv.size() == 0);
    REQUIRE(Tracker::destructions == 1); // Destructor for Tracker(10)
}

TEST_CASE("SmallVector: Access and Bounds (int, N=3)", "[small_vector]") {
    SmallVector<int, 3> sv_stack = {10, 20};
    REQUIRE(sv_stack[0] == 10);
    REQUIRE(sv_stack.at(1) == 20);
    REQUIRE_THROWS_AS(sv_stack[2], std::out_of_range);
    REQUIRE_THROWS_AS(sv_stack.at(2), std::out_of_range);

    const SmallVector<int, 3> csv_stack = {10, 20};
    REQUIRE(csv_stack[0] == 10);
    REQUIRE(csv_stack.at(1) == 20);
    REQUIRE_THROWS_AS(csv_stack[2], std::out_of_range);
    REQUIRE_THROWS_AS(csv_stack.at(2), std::out_of_range);
    
    SmallVector<int, 1> sv_heap = {10, 20, 30};
    REQUIRE(sv_heap[0] == 10);
    REQUIRE(sv_heap.at(2) == 30);
    REQUIRE_THROWS_AS(sv_heap[3], std::out_of_range);
    REQUIRE_THROWS_AS(sv_heap.at(3), std::out_of_range);
}

TEST_CASE("SmallVector: Clear (int, N=2)", "[small_vector]") {
    SECTION("Clear on stack") {
        SmallVector<int, 2> sv = {1};
        REQUIRE(sv.size() == 1);
        sv.clear();
        REQUIRE(sv.size() == 0);
        REQUIRE(sv.empty());
        REQUIRE(sv.on_stack()); // Should remain on stack
        REQUIRE(sv.capacity() == 2); // Capacity usually doesn't change on clear
    }

    SECTION("Clear on heap") {
        SmallVector<int, 2> sv = {1, 2, 3};
        REQUIRE(sv.size() == 3);
        REQUIRE_FALSE(sv.on_stack());
        size_t old_capacity = sv.capacity();
        
        sv.clear();
        REQUIRE(sv.size() == 0);
        REQUIRE(sv.empty());
        REQUIRE_FALSE(sv.on_stack()); // Stays on heap
        REQUIRE(sv.capacity() == old_capacity); // Capacity doesn't change
    }
}

TEST_CASE("SmallVector: Copy Semantics (Tracker, N=2)", "[small_vector]") {
    Tracker::reset_counts();
    SECTION("Copy construct on stack to on stack") {
        SmallVector<Tracker, 2> sv1;
        sv1.push_back(Tracker(1));
        sv1.push_back(Tracker(2));
        Tracker::reset_counts(); // Reset after setup

        SmallVector<Tracker, 2> sv2 = sv1;
        REQUIRE(sv2.size() == 2);
        REQUIRE(sv2.on_stack());
        REQUIRE(Tracker::copies == 2); // Each element copied
        REQUIRE(sv2[0].id == 1);
        REQUIRE(sv2[1].id == 2);
    }
    
    SECTION("Copy construct on heap to on heap") {
        SmallVector<Tracker, 2> sv1;
        sv1.push_back(Tracker(1));
        sv1.push_back(Tracker(2));
        sv1.push_back(Tracker(3)); // sv1 is on heap
        Tracker::reset_counts();

        SmallVector<Tracker, 2> sv2 = sv1;
        REQUIRE(sv2.size() == 3);
        REQUIRE_FALSE(sv2.on_stack());
        REQUIRE(Tracker::copies == 3);
        REQUIRE(sv2[2].id == 3);
    }

    SECTION("Copy assignment on stack to on stack") {
        SmallVector<Tracker, 2> sv1;
        sv1.push_back(Tracker(1));
        SmallVector<Tracker, 2> sv2;
        sv2.push_back(Tracker(10));
        sv2.push_back(Tracker(20));
        Tracker::reset_counts();

        sv1 = sv2; // sv1 (size 1) = sv2 (size 2)
        REQUIRE(sv1.size() == 2);
        REQUIRE(sv1.on_stack());
        // Expect 1 destruction (old sv1[0]) + 2 copies (sv2[0], sv2[1])
        REQUIRE(Tracker::destructions >= 1); 
        REQUIRE(Tracker::copies == 2); 
        REQUIRE(sv1[1].id == 20);
    }

    SECTION("Copy assignment on heap to on heap") {
        SmallVector<Tracker, 2> sv1 = {Tracker(1), Tracker(2), Tracker(3)};
        SmallVector<Tracker, 2> sv2 = {Tracker(10), Tracker(20), Tracker(30), Tracker(40)};
        Tracker::reset_counts();

        sv1 = sv2; // sv1 (heap, size 3) = sv2 (heap, size 4)
        REQUIRE(sv1.size() == 4);
        REQUIRE_FALSE(sv1.on_stack());
        REQUIRE(Tracker::destructions >= 3); // Old elements of sv1
        REQUIRE(Tracker::copies == 4);    // New elements from sv2
        REQUIRE(sv1[3].id == 40);
    }
}

TEST_CASE("SmallVector: Move Semantics (Tracker, N=2)", "[small_vector]") {
    Tracker::reset_counts();
    SECTION("Move construct from stack") {
        SmallVector<Tracker, 2> sv1;
        sv1.push_back(Tracker(1)); 
        Tracker::reset_counts();

        SmallVector<Tracker, 2> sv2 = std::move(sv1);
        REQUIRE(sv2.size() == 1);
        REQUIRE(sv2.on_stack());
        REQUIRE(sv2[0].id == 1);
        REQUIRE(Tracker::moves >= 1); // Element(s) moved
        REQUIRE(Tracker::copies == 0);
        REQUIRE(sv1.empty()); // sv1 should be empty or in a valid but unspecified state
        REQUIRE(sv1.on_stack()); // sv1 should remain on stack if it was, now empty
    }

    SECTION("Move construct from heap") {
        SmallVector<Tracker, 2> sv1;
        sv1.push_back(Tracker(1));
        sv1.push_back(Tracker(2));
        sv1.push_back(Tracker(3)); // sv1 on heap
        Tracker::reset_counts();

        SmallVector<Tracker, 2> sv2 = std::move(sv1);
        REQUIRE(sv2.size() == 3);
        REQUIRE_FALSE(sv2.on_stack());
        REQUIRE(sv2[2].id == 3);
        REQUIRE(Tracker::moves == 0); // Pointer swap for heap data, no element-wise moves
        REQUIRE(Tracker::copies == 0);
        REQUIRE(sv1.empty()); // Or valid unspecified state
        REQUIRE(sv1.on_stack()); // After move, sv1's heap buffer is gone, it should revert to stack state
    }
    
    SECTION("Move assignment to stack from stack") {
        SmallVector<Tracker, 2> sv1; sv1.push_back(Tracker(1));
        SmallVector<Tracker, 2> sv2; sv2.push_back(Tracker(10)); sv2.push_back(Tracker(20));
        Tracker::reset_counts();

        sv2 = std::move(sv1);
        REQUIRE(sv2.size() == 1);
        REQUIRE(sv2.on_stack());
        REQUIRE(sv2[0].id == 1);
        REQUIRE(Tracker::destructions >= 2); // sv2's old elements
        REQUIRE(Tracker::moves >= 1);     // sv1's element
        REQUIRE(Tracker::copies == 0);
    }


    SECTION("Move assignment to heap from heap") {
        SmallVector<Tracker, 2> sv1 = {Tracker(1), Tracker(2), Tracker(3)}; // sv1 on heap
        SmallVector<Tracker, 2> sv2 = {Tracker(10), Tracker(20), Tracker(30), Tracker(40)}; // sv2 on heap
        Tracker::reset_counts();

        sv2 = std::move(sv1); // sv2's old heap data should be released
        REQUIRE(sv2.size() == 3);
        REQUIRE_FALSE(sv2.on_stack());
        REQUIRE(sv2[0].id == 1);
        REQUIRE(Tracker::destructions >= 4); // sv2's old 4 elements
        REQUIRE(Tracker::moves == 0);      // Pointer swap
        REQUIRE(Tracker::copies == 0);
        REQUIRE(sv1.empty()); // Or valid unspecified state
        REQUIRE(sv1.on_stack());
    }
}

TEST_CASE("SmallVector: Growth from stack to heap (Tracker, N=1)", "[small_vector]") {
    Tracker::reset_counts();
    SmallVector<Tracker, 1> sv;
    
    sv.push_back(Tracker(100)); // On stack
    REQUIRE(sv.on_stack());
    REQUIRE(Tracker::constructions == 1); // Tracker(100)
    REQUIRE(Tracker::moves == 0);      // No element-wise move yet
    REQUIRE(Tracker::copies == 0);
    Tracker::reset_counts();

    sv.push_back(Tracker(200)); // Should move to heap
    REQUIRE_FALSE(sv.on_stack());
    REQUIRE(sv.size() == 2);
    REQUIRE(sv[0].id == 100);
    REQUIRE(sv[1].id == 200);

    // When moving from stack to heap:
    // 1. New Tracker(200) is constructed (or moved into place if push_back takes by value and moves).
    // 2. Old Tracker(100) from stack is MOVED to the new heap buffer.
    // 3. Old Tracker(100) on stack is destructed.
    REQUIRE(Tracker::constructions >= 1); // For Tracker(200)
    REQUIRE(Tracker::moves >= 1);      // For moving Tracker(100) from stack to heap
    REQUIRE(Tracker::destructions >= 1); // For Tracker(100) on stack
}


TEST_CASE("SmallVector: Iterators (int, N=3)", "[small_vector]") {
    SECTION("Iterating on stack") {
        SmallVector<int, 3> sv = {10, 20};
        int sum = 0;
        for (auto it = sv.begin(); it != sv.end(); ++it) {
            sum += *it;
        }
        REQUIRE(sum == 30);

        sum = 0;
        for (int x : sv) { // Range-based for
            sum += x;
        }
        REQUIRE(sum == 30);
    }

    SECTION("Iterating on heap") {
        SmallVector<int, 2> sv = {10, 20, 30, 40};
        int sum = 0;
        for (const auto& val : sv) {
            sum += val;
        }
        REQUIRE(sum == 100);
    }
    
    SECTION("Modifying through iterator") {
        SmallVector<int, 3> sv = {1,2,3};
        for(auto it = sv.begin(); it != sv.end(); ++it) {
            *it = *it * 10;
        }
        REQUIRE(sv[0] == 10);
        REQUIRE(sv[1] == 20);
        REQUIRE(sv[2] == 30);
    }
}

// TODO (Potentially, if time allows or for further depth):
// - emplace_back tests
// - insert tests
// - erase tests
// - resize tests
// - shrink_to_fit (and whether it can go from heap back to stack)
// - Exception safety guarantees (e.g. strong guarantee for push_back if T's move ctor doesn't throw)
// - Allocator support (making SmallVector allocator-aware so it could use your ArenaAllocator for heap part)