// tests/concurrency_stress_tests.cpp
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <arena_allocator.h>
#include <small_vector.h>

#include <thread>
#include <atomic>
#include <numeric>
#include <random>
#include <unordered_set>

using byte = std::byte;

/*------------------------------------------------------------
  Stress #1 :   “Hungry Hamsters”
  Many threads allocate random-sized chunks out of ONE arena.
  After the run we scan for overlapping ranges.  Crash occurs
  rarely, so the test retries until it reproduces (or times out).
------------------------------------------------------------*/

namespace {

// one record per successful allocation
struct ChunkInfo {
    byte* begin;
    byte* end;
};

constexpr size_t N = 1'000'000;

// helper: detect overlaps in O(n log n)
bool any_overlap(SmallVector<ChunkInfo, 1024>& chunks)
{
    std::sort(chunks.begin(), chunks.end(),
              [](auto& a, auto& b){ return a.begin < b.begin; });
    for (size_t i = 1; i < chunks.size(); ++i)
        if (chunks[i-1].end > chunks[i].begin) { 
            return true;
        }
    return false;
}

void hamster_worker(ArenaAllocator& arena,
                    SmallVector<ChunkInfo, 128>& out_chunks,
                    std::mt19937& rng,
                    std::atomic<bool>& start_flag,
                    size_t allocs_per_thread)
{
    std::uniform_int_distribution<int> size_dist(8, 128);
    while (!start_flag.load(std::memory_order_relaxed)) /* spin */;

    for (size_t n = 0; n < allocs_per_thread; ++n)
    {
        size_t sz = static_cast<size_t>(size_dist(rng));
        void* p = arena.allocate(sz, alignof(std::max_align_t));
        if (p == nullptr) break;               // arena exhausted
        out_chunks.push_back({ static_cast<byte*>(p),
                               static_cast<byte*>(p) + sz });
    }
}

} // namespace

TEST_CASE("ArenaAllocator : Hungry Hamsters", "[thread][stress]")
{
    constexpr size_t ARENA_SIZE = 2 * 1024 * 1024;          // 2 MB
    constexpr size_t THREAD_ALLOCS = 10'000;

    ArenaAllocator arena(ARENA_SIZE);

    const unsigned P = std::thread::hardware_concurrency();
    SmallVector<SmallVector<ChunkInfo, 128>, 32> all_chunks; // per-thread scratch
    for (unsigned t = 0; t < P; ++t) all_chunks.push_back({});

    std::atomic<bool> start{false};
    std::vector<std::thread> crew;
    std::vector<std::mt19937> rngs;
    for (unsigned t = 0; t < P; ++t)
        rngs.emplace_back(std::random_device{}());

    for (unsigned t = 0; t < P; ++t)
    {
        crew.emplace_back(hamster_worker,
                          std::ref(arena),
                          std::ref(all_chunks[t]),
                          std::ref(rngs[t]),
                          std::ref(start),
                          THREAD_ALLOCS);
    }

    start.store(true, std::memory_order_relaxed);
    for (auto& th : crew) th.join();

    // flatten into one big SmallVector so we can sort/scan
    SmallVector<ChunkInfo, 1024> flat;
    for (auto& v : all_chunks)
        for (auto& c : v) flat.push_back(c);

    // occasional UB shows up as overlaps (or seg-fault long before here)
    REQUIRE_FALSE(any_overlap(flat));   // <-- intermittent failure
}
