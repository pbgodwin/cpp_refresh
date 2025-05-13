#include <catch2/catch_test_macros.hpp>

#include <unique_buffer.h>

TEST_CASE("Default ctor yields empty buffer") {
    UniqueBuffer<int> m_buffer;
    REQUIRE( m_buffer.size() == 0 );
    REQUIRE( m_buffer.data() == nullptr);
}

TEST_CASE("Initial size ctor allocates buffer") {
    UniqueBuffer<int> m_buffer(2);
    REQUIRE( m_buffer.size() == 2 );
    REQUIRE_FALSE( m_buffer.data() == nullptr );
}

TEST_CASE("Move ctor leaves source null") {
    UniqueBuffer<int> m_srcBuffer(1);
    UniqueBuffer<int> m_dest = std::move(m_srcBuffer);
    REQUIRE( m_srcBuffer.size() == 0 );
    REQUIRE( m_srcBuffer.data() == nullptr );
    REQUIRE( m_dest.size() == 1 );
    REQUIRE_FALSE( m_dest.data() == nullptr );
}

TEST_CASE("Move assignment leaves source null") {
    UniqueBuffer<int> m_srcBuffer(1);
    UniqueBuffer<int> m_dest;
    m_dest = std::move(m_srcBuffer);
    REQUIRE( m_srcBuffer.size() == 0 );
    REQUIRE( m_srcBuffer.data() == nullptr );
    REQUIRE( m_dest.size() == 1 );
    REQUIRE_FALSE( m_dest.data() == nullptr );
}

TEST_CASE("Dtor does not allow double free") {
    
}