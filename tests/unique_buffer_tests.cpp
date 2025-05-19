#include <catch2/catch_test_macros.hpp>

#include <unique_buffer.h>

TEST_CASE("UniqueBuffer: Default ctor yields empty buffer") {
    UniqueBuffer<int> m_buffer;
    REQUIRE( m_buffer.size() == 0 );
    REQUIRE( m_buffer.data() == nullptr);
}

TEST_CASE("UniqueBuffer: Initial size ctor allocates buffer") {
    UniqueBuffer<int> m_buffer(2);
    REQUIRE( m_buffer.size() == 2 );
    REQUIRE_FALSE( m_buffer.data() == nullptr );
}

TEST_CASE("UniqueBuffer: Move ctor leaves source null") {
    UniqueBuffer<int> m_srcBuffer(1);
    UniqueBuffer<int> m_dest = std::move(m_srcBuffer);
    REQUIRE( m_srcBuffer.size() == 0 );
    REQUIRE( m_srcBuffer.data() == nullptr );
    REQUIRE( m_dest.size() == 1 );
    REQUIRE_FALSE( m_dest.data() == nullptr );
}

TEST_CASE("UniqueBuffer: Move assignment leaves source null") {
    UniqueBuffer<int> m_srcBuffer(1);
    UniqueBuffer<int> m_dest;
    m_dest = std::move(m_srcBuffer);
    REQUIRE( m_srcBuffer.size() == 0 );
    REQUIRE( m_srcBuffer.data() == nullptr );
    REQUIRE( m_dest.size() == 1 );
    REQUIRE_FALSE( m_dest.data() == nullptr );
}

TEST_CASE("UniqueBuffer: array accessor throws when out of range") {
    UniqueBuffer<int> buf(3);
    REQUIRE_THROWS_AS(buf[3], std::out_of_range);
}

/*
TEST_CASE("UniqueBuffer: Const usage, only for compile example") {
    const UniqueBuffer<int> cbuf(2);
    auto s = cbuf.size();      // should compile
    REQUIRE( s == 2 );
    cbuf[0] = 1;               // should fail to compile
}*/