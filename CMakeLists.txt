cmake_minimum_required(VERSION 3.10.0)
project(interview-prep VERSION 0.1.0 LANGUAGES C CXX)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_executable(interview-prep 
  tests/main.cpp
  tests/unique_buffer_tests.cpp
  src/unique_buffer.cpp
)
target_include_directories(interview-prep PRIVATE inc)
target_link_libraries(interview-prep PRIVATE Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(interview-prep)
enable_testing()

