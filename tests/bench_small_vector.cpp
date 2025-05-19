#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <small_vector.h>

TEST_CASE("push_back vs std::vector", "[bench]") {

    constexpr std::size_t N = 1'000;

    BENCHMARK("SmallVector<N=8> push_back N ints") {
        SmallVector<int, 8> sv;
        for (std::size_t i = 0; i < N; ++i) sv.push_back(static_cast<int>(i));
        return sv.size();               // return something so Catch2 can't dead-strip
    };

    BENCHMARK("SmallVector<N=1'000> push_back N ints") {
        SmallVector<int, 1'000> sv;
        for (std::size_t i = 0; i < N; ++i) sv.push_back(static_cast<int>(i));
        return sv.size();               // return something so Catch2 can't dead-strip
    };

    BENCHMARK("std::vector push_back N ints") {
        std::vector<int> v;
        v.reserve(8);                   // same starting capacity
        for (std::size_t i = 0; i < N; ++i) v.push_back(static_cast<int>(i));
        return v.size();
    };
}
