cmake_minimum_required(VERSION 3.15.0)
project(cpp_refresh VERSION 0.1.0 LANGUAGES C CXX)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)


add_executable(cpp_refresh 
  tests/main.cpp
  tests/unique_buffer_tests.cpp
  tests/arena_allocator_tests.cpp
  tests/small_vector_tests.cpp
)
target_include_directories(cpp_refresh PRIVATE inc)
target_link_libraries(cpp_refresh PRIVATE Catch2::Catch2WithMain)

# Enable warnings-as-errors specifically for your cpp_refresh target
if(MSVC)
    target_compile_options(cpp_refresh PRIVATE "/WX")
else() # Assuming GCC/Clang or compatible
    target_compile_options(cpp_refresh PRIVATE "-Werror")
endif()

set(CMAKE_BUILD_TYPE "Debug")

target_compile_features(cpp_refresh PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(cpp_refresh)
enable_testing()
