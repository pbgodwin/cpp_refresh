cmake_minimum_required(VERSION 3.10.0)
project(interview_prep VERSION 0.1.0 LANGUAGES C CXX)

set(CLANG_COVERAGE_LINK_FLAGS    "--coverage")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CLANG_COVERAGE_LINK_FLAGS}" )

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)


add_executable(interview_prep 
  tests/main.cpp
  tests/unique_buffer_tests.cpp
)
target_include_directories(interview_prep PRIVATE inc)
target_link_libraries(interview_prep PRIVATE Catch2::Catch2WithMain)

# Enable warnings-as-errors specifically for your interview_prep target
if(MSVC)
    target_compile_options(interview_prep PRIVATE "/WX")
else() # Assuming GCC/Clang or compatible
    target_compile_options(interview_prep PRIVATE "-Werror")
endif()

set(CMAKE_BUILD_TYPE "Debug")

# compile with coverage options
target_compile_options(interview_prep PRIVATE -fprofile-instr-generate -fcoverage-mapping)
target_link_libraries(interview_prep PRIVATE -fprofile-instr-generate -fcoverage-mapping)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(interview_prep)
enable_testing()

