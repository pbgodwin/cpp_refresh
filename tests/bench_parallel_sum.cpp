#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <small_vector.h>
#include <thread>
#include <numeric>
#include <atomic>
#include <vector>

float parallel_sum(SmallVector<float, 8>& vec) {
    unsigned p = std::thread::hardware_concurrency();
    size_t chunk = vec.size() / p;
    std::atomic<float> total{0};

    std::vector<std::thread> workers;
    for (unsigned t = 0; t < p; ++t) {
        size_t begin = t * chunk;
        size_t end   = (t == p - 1) ? vec.size() : begin + chunk;
        workers.emplace_back([&, begin, end]{
            float local = 0;
            for (size_t i = begin; i < end; ++i) local += vec[i];
            float cur_total = total.load(std::memory_order_relaxed);
            cur_total += local;
            total.store(cur_total);
        });
    }
    for (auto& th : workers) th.join();
    return total.load();
}

TEST_CASE("SmallVector: Parallel sum vs scalar", "[bench][thread]") {
    const size_t N = 1'000'000;
    SmallVector<float, 8> v;
    for (size_t i = 0; i < N; ++i) v.push_back(static_cast<float>(i));

    BENCHMARK("scalar sum") {
        return std::accumulate(v.begin(), v.end(), 0.0f);
    };

    BENCHMARK("parallel sum") {
        return parallel_sum(v);
    };
}
