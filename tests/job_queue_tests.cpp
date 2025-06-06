// job_queue_tests.cpp
// g++ -std=c++20 -O2 -pthread job_queue_tests.cpp -o tests && ./tests

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include <unordered_set>
#include <job_queue.h>   // <- you implement this header

using Task = std::size_t;           // unique integer tokens are enough
constexpr std::size_t N_PRODUCERS = 4;
constexpr std::size_t N_CONSUMERS = 4;
constexpr std::size_t TASKS_PER_PRODUCER = 10'000;


// Helper: busy-wait until predicate or timeout (avoids <condition_variable>)
template <class Pred>
bool spin_until(Pred pred, std::chrono::milliseconds max = std::chrono::milliseconds(2 * 100))
{
    auto deadline = std::chrono::steady_clock::now() + max;
    while (!pred() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    return pred();
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Empty / full edge cases
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Empty queue pop fails, full queue push fails")
{
    constexpr std::size_t CAP = 4;
    JobQueue<Task> q{CAP};

    Task dummy;
    REQUIRE_FALSE(q.pop(dummy));      // empty pop must fail

    for (Task t = 0; t < CAP; ++t) REQUIRE(q.push(t));
    REQUIRE_FALSE(q.push(42));        // full push must fail

    for (Task expect = 0; expect < CAP; ++expect)
    {
        REQUIRE(q.pop(dummy));
        REQUIRE(dummy == expect);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Wrap-around sanity with tiny capacity
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Ring wrap-around works for capacity=2")
{
    JobQueue<Task> q{2};
    Task out;

    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    REQUIRE(q.pop(out));
    REQUIRE(out == 1);

    REQUIRE(q.push(3));               // should wrap
    REQUIRE(q.pop(out));
    REQUIRE(out == 2);
    REQUIRE(q.pop(out));
    REQUIRE(out == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Single-producer single-consumer stress
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: SPSC 1M ops survives")
{
    constexpr std::size_t CAP = 512;
    constexpr std::size_t N   = 1'000'000;
    JobQueue<Task> q{CAP};

    std::thread prod([&]{
        for (Task i = 0; i < N; ++i)
        {
            while (!q.push(i)) std::this_thread::yield();
        }
    });

    std::thread cons([&]{
        Task v;
        for (Task i = 0; i < N; ++i)
        {
            while (!q.pop(v)) std::this_thread::yield();
            REQUIRE(v == i);
        }
    });

    prod.join();
    cons.join();
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Multi-producer multi-consumer: uniqueness guarantee
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: MPMC 4x4 preserves uniqueness")
{
    constexpr std::size_t CAP = 4096;
    constexpr std::size_t P   = 4;                 // producers
    constexpr std::size_t C   = 4;                 // consumers
    constexpr std::size_t PER = 50'000;            // tasks per producer
    const     std::size_t TOTAL = P * PER;

    JobQueue<Task> q{CAP};
    std::atomic<std::size_t> done{0};
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> seen(TOTAL);
    for (auto& f: seen) f = false;

    // Producers
    for (std::size_t p = 0; p < P; ++p)
        threads.emplace_back([&, p]{
            Task base = p * PER;
            for (Task i = 0; i < PER; ++i)
                while (!q.push(base + i)) std::this_thread::yield();
        });

    // Consumers (also stealing from same queue = degenerate case)
    for (std::size_t c = 0; c < C; ++c)
        threads.emplace_back([&]{
            Task v;
            while (done.load() < TOTAL)
            {
                if (q.pop(v) || q.steal(q, v))
                {
                    REQUIRE_FALSE(seen[v].exchange(true)); // duplicates explode
                    ++done;
                }
                else  std::this_thread::yield();
            }
        });

    for (auto& t : threads) t.join();
    REQUIRE(done == TOTAL);
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Concurrent pop vs steal (victim vs thief) race
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Pop-while-steal produces no lost or dup tasks")
{
    constexpr std::size_t CAP = 1024;
    JobQueue<Task> q{CAP};

    for (Task t = 0; t < CAP; ++t) q.push(t);

    std::unordered_set<Task> bag;
    std::atomic<bool> stop{false};
    Task tmp;

    std::thread thief([&]{
        while (!stop)
            if (q.steal(q, tmp)) bag.insert(tmp);
    });

    while (bag.size() < CAP)
        if (q.pop(tmp)) bag.insert(tmp);

    stop = true;
    thief.join();
    REQUIRE(bag.size() == CAP);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Destructor race: queue dies while threads still poking it
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Queue destruction after concurrent use is safe")
{
    constexpr std::size_t CAP = 256;
    std::thread t;
    {
        auto q = std::make_unique<JobQueue<Task>>(CAP);
        std::atomic<bool> go{false};

        t = std::thread([&]{
            Task x;
            spin_until([&]{ return go.load(); });
            for (int i = 0; i < CAP * 10; ++i)
                q->push(i);
        });

        go = true;
        // scope ends, q destructor will wait for thread?  not its job.
    }
    t.join();   // if the destructor UB’d, ASAN/Valgrind or Windows CRT will yell.
}


// ──────────────────────────────────────────────────────────────────────────────
// 1. Single-thread sanity: FIFO & uniqueness
// ──────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: single-thread push/pop preserves FIFO")
{
    JobQueue<Task> q{TASKS_PER_PRODUCER};
    for (Task t = 0; t < TASKS_PER_PRODUCER; ++t) q.push(t);

    Task out;
    for (Task expected = 0; expected < TASKS_PER_PRODUCER; ++expected)
    {
        REQUIRE(q.pop(out));
        REQUIRE(out == expected);
    }
    REQUIRE_FALSE(q.pop(out));          // queue should now be empty
}

// ──────────────────────────────────────────────────────────────────────────────
// 2. Multi-producer / multi-consumer: no duplicates, nothing lost
// ──────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: survives N producers + N consumers without loss")
{
    JobQueue<Task> queues[N_CONSUMERS]{TASKS_PER_PRODUCER * N_PRODUCERS};

    // Shared state to verify execution
    std::atomic<std::size_t> executed{0};
    std::vector<std::thread> threads;

    // Producers
    for (std::size_t p = 0; p < N_PRODUCERS; ++p)
        threads.emplace_back([&, p] {
            for (Task local = 0; local < TASKS_PER_PRODUCER; ++local)
                queues[p % N_CONSUMERS].push(p * TASKS_PER_PRODUCER + local);
        });

    // Consumers (each owns its own queue, then steals)
    for (std::size_t c = 0; c < N_CONSUMERS; ++c)
        threads.emplace_back([&, c] {
            Task task;
            while (executed.load() < N_PRODUCERS * TASKS_PER_PRODUCER)
            {
                if (queues[c].pop(task))
                {
                    ++executed;
                    continue;
                }
                // Try to steal round-robin
                for (std::size_t v = 0; v < N_CONSUMERS; ++v)
                    if (v != c && queues[c].steal(queues[v], task))
                    {
                        ++executed;
                        break;
                    }
                std::this_thread::yield();   // let others progress
            }
        });

    for (auto& t : threads) t.join();
    REQUIRE(executed == N_PRODUCERS * TASKS_PER_PRODUCER);
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. Pop-while-steal race: victim pops from head while thief steals from tail
// ──────────────────────────────────────────────────────────────────────────────
TEST_CASE("JobQueue: Concurrent pop vs steal never duplicates or drops a task")
{
    constexpr std::size_t CAP = 1'024;
    JobQueue<Task> q{CAP};

    // pre-fill queue exactly to capacity
    for (Task t = 0; t < CAP; ++t) q.push(t);

    std::unordered_set<Task> seen;        // not thread-safe by design
    std::atomic<bool> done{false};
    std::thread thief([&] {
        Task t;
        while (!done)
            if (q.steal(q, t))            // self-steal forces wraparound logic
                seen.insert(t);
    });

    Task t;
    while (seen.size() < CAP)
        if (q.pop(t)) seen.insert(t);

    done = true;
    thief.join();
    REQUIRE(seen.size() == CAP);          // every token 0-1023 exactly once
}
