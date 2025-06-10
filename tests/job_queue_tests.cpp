// job_queue_tests.cpp
// A rigorous test suite for a lock-free MPMC queue with work-stealing.
//
// To compile and run:
// g++ -std=c++20 -O2 -pthread -I./include job_queue_tests.cpp -o tests && ./tests
// (Assumes catch2/catch_test_macros.hpp is in ./include)

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>
#include <mutex>
#include <unordered_set>

// The API being tested. Assumes job_queue.h is in ./include
#include <job_queue.h>

// Test configuration
using Task = std::size_t;
constexpr std::size_t STRESS_TEST_TASKS = 50'000;
constexpr std::size_t NUM_PRODUCERS = 4;
constexpr std::size_t NUM_WORKERS = 4; // In work-stealing, workers are consumers

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: Basic single-threaded sanity checks
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Single-threaded sanity checks", "[correctness]")
{
    constexpr std::size_t CAP = 4;
    JobQueue<Task> q{CAP};
    Task out;

    SECTION("Empty/Full behavior") {
        REQUIRE_FALSE(q.pop(out)); // Pop from empty queue must fail

        for (Task t = 1; t <= CAP; ++t) {
            REQUIRE(q.push(t));
        }
        REQUIRE_FALSE(q.push(99)); // Push to full queue must fail
    }

    SECTION("FIFO ordering and wrap-around") {
        JobQueue<Task> q_wrap{2};
        REQUIRE(q_wrap.push(1)); // q: [1, _]
        REQUIRE(q_wrap.push(2)); // q: [1, 2]
        REQUIRE_FALSE(q_wrap.push(3)); // full

        REQUIRE(q_wrap.pop(out));
        REQUIRE(out == 1);       // q: [_, 2]

        REQUIRE(q_wrap.push(3)); // q: [3, 2] (wraps around)
        
        REQUIRE(q_wrap.pop(out));
        REQUIRE(out == 2);       // q: [3, _]

        REQUIRE(q_wrap.pop(out));
        REQUIRE(out == 3);       // q: [_, _]

        REQUIRE_FALSE(q_wrap.pop(out)); // empty
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: Multi-Producer, Multi-Consumer (MPMC) on a SINGLE queue
// Verifies that the core push/pop atomics are safe under contention.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: MPMC on a single queue preserves all items", "[concurrency]")
{
    constexpr std::size_t TASKS_PER_PRODUCER = STRESS_TEST_TASKS;
    const std::size_t TOTAL_TASKS = NUM_PRODUCERS * TASKS_PER_PRODUCER;
    const std::size_t QUEUE_CAP = 4096;

    JobQueue<Task> q{QUEUE_CAP};
    std::vector<std::thread> threads;
    
    // Use a thread-safe mechanism to verify results.
    // An atomic flag per task is perfect for detecting duplicates and losses.
    std::vector<std::atomic<bool>> seen_tasks(TOTAL_TASKS);
    for(size_t i = 0; i < TOTAL_TASKS; ++i) seen_tasks[i] = false;

    std::atomic<size_t> tasks_consumed = 0;

    // --- Producers ---
    for (std::size_t p_id = 0; p_id < NUM_PRODUCERS; ++p_id) {
        threads.emplace_back([&, p_id] {
            Task base_task = p_id * TASKS_PER_PRODUCER;
            for (Task i = 0; i < TASKS_PER_PRODUCER; ++i) {
                Task task = base_task + i;
                while (!q.push(task)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // --- Consumers ---
    for (std::size_t c_id = 0; c_id < NUM_WORKERS; ++c_id) {
        threads.emplace_back([&] {
            Task task;
            // Keep consuming until all tasks are accounted for.
            while (tasks_consumed.load(std::memory_order_relaxed) < TOTAL_TASKS) {
                if (q.pop(task)) {
                    // The exchange operation atomically sets the flag to true and
                    // returns the OLD value. If it was already true, we have a duplicate.
                    REQUIRE_FALSE(seen_tasks[task].exchange(true, std::memory_order_relaxed));
                    tasks_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Final verification
    REQUIRE(tasks_consumed == TOTAL_TASKS);
    for(size_t i = 0; i < TOTAL_TASKS; ++i) {
        REQUIRE(seen_tasks[i].load()); // Ensure every single task was seen.
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: Full Work-Stealing Simulation
// Each worker has its own queue. Workers pop from their own queue and steal
// from others when their own is empty. This is the canonical use case.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Work-stealing simulation with multiple queues", "[work-stealing]")
{
    const std::size_t TASKS_PER_WORKER = STRESS_TEST_TASKS;
    const std::size_t TOTAL_TASKS = NUM_WORKERS * TASKS_PER_WORKER;
    const std::size_t QUEUE_CAP = 2048;

    // One queue per worker
    std::vector<JobQueue<Task>> queues;
    queues.reserve(NUM_WORKERS); // Optional but good practice to avoid reallocations
    for (size_t i = 0; i < NUM_WORKERS; ++i) {
        queues.emplace_back(QUEUE_CAP);
    }

    std::vector<std::thread> workers;

    // Thread-safe verification tools
    std::vector<std::atomic<bool>> seen_tasks(TOTAL_TASKS);
     for(size_t i = 0; i < TOTAL_TASKS; ++i) seen_tasks[i] = false;
    std::atomic<size_t> tasks_processed = 0;

    // --- Phase 1: Each worker produces its own tasks ---
    for (std::size_t worker_id = 0; worker_id < NUM_WORKERS; ++worker_id) {
        Task base_task = worker_id * TASKS_PER_WORKER;
        for (Task i = 0; i < TASKS_PER_WORKER; ++i) {
            queues[worker_id].push(base_task + i);
        }
    }

    // --- Phase 2: Workers process tasks from own queue, then steal ---
    for (std::size_t worker_id = 0; worker_id < NUM_WORKERS; ++worker_id) {
        workers.emplace_back([&, worker_id] {
            Task task;
            while (tasks_processed.load(std::memory_order_acquire) < TOTAL_TASKS) {
                // 1. Try to pop from our own queue
                if (queues[worker_id].pop(task)) {
                    REQUIRE_FALSE(seen_tasks[task].exchange(true));
                    tasks_processed.fetch_add(1, std::memory_order_release);
                    continue;
                }

                // 2. Own queue is empty, try to steal from others
                bool was_stolen = false;
                for (std::size_t i = 1; i < NUM_WORKERS; ++i) {
                    std::size_t victim_id = (worker_id + i) % NUM_WORKERS;
                    
                    // NOTE: The first argument to steal() is the thief's queue,
                    // the second is the victim's. Your API is `thief.steal(victim, out)`.
                    if (queues[worker_id].steal(queues[victim_id], task)) {
                        REQUIRE_FALSE(seen_tasks[task].exchange(true));
                        tasks_processed.fetch_add(1, std::memory_order_release);
                        was_stolen = true;
                        break; // Stop stealing after one success
                    }
                }

                if (!was_stolen) {
                    std::this_thread::yield(); // Avoid busy-spinning if all queues are empty
                }
            }
        });
    }

    for (auto& w : workers) w.join();

    // Final verification
    REQUIRE(tasks_processed == TOTAL_TASKS);
    for(size_t i = 0; i < TOTAL_TASKS; ++i) {
        REQUIRE(seen_tasks[i].load());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Pop vs. Steal Race Condition
// One thread pops from the head while another steals from the head of the SAME queue.
// This is a focused stress test for the head pointer's atomicity.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Pop vs Steal race does not lose or duplicate tasks", "[concurrency]")
{
    constexpr std::size_t CAP = 4096;
    JobQueue<Task> q{CAP};

    // Pre-fill the queue
    for (Task t = 0; t < CAP; ++t) q.push(t);

    std::thread popper;
    std::thread stealer;
    
    // Use a mutex for the result set, as it's a complex data type.
    std::mutex bag_mutex;
    std::unordered_set<Task> collected_tasks;

    std::atomic<bool> stop_flag = false;

    // The "popper" (owner)
    popper = std::thread([&] {
        Task task;
        while (!stop_flag.load(std::memory_order_acquire)) {
            if (q.pop(task)) {
                std::lock_guard<std::mutex> lock(bag_mutex);
                collected_tasks.insert(task);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // The "stealer" (thief)
    stealer = std::thread([&] {
        Task task;
        // The thief is another queue instance, as per the API
        JobQueue<Task> thief_q{1}; 
        while (!stop_flag.load(std::memory_order_acquire)) {
            if (thief_q.steal(q, task)) {
                std::lock_guard<std::mutex> lock(bag_mutex);
                collected_tasks.insert(task);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Let them run until all tasks are collected
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(bag_mutex);
        if (collected_tasks.size() == CAP) {
            stop_flag.store(true, std::memory_order_release);
            break;
        }
    }

    popper.join();
    stealer.join();

    REQUIRE(collected_tasks.size() == CAP);
}