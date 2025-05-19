# cpp_refresh

## Synopsis

A C++ project focused on implementing and exploring various data structures and memory management techniques in modern C++. It serves as a practical exercise for C++17 features and common patterns found in performance-sensitive applications.

## Core Components

*   **`UniqueBuffer`**: A RAII-compliant, move-only buffer that manages a dynamically allocated array. It provides safe and exclusive ownership of a memory block.
*   **`ArenaAllocator`**: A custom memory allocator that pre-allocates a fixed-size memory region (arena) and services allocation requests from this region. This can improve performance by reducing individual heap allocations and improving memory locality.
*   **`SmallVector`**: A sequence container similar to `std::vector` but optimized for a small number of elements by initially using stack-allocated storage. It transitions to heap allocation if the number of elements exceeds its initial stack capacity.

## Building the Project

### Prerequisites

*   A C++17 compliant compiler (e.g., GCC, Clang, MSVC).
*   CMake (version 3.15 or higher).

### Steps

```bash
git clone https://github.com/pbgodwin/cpp_refresh # You'll need to replace this with your actual repo URL
cd cpp_refresh
cmake -B build -S .
cmake --build build
```

## Running Tests

The project uses Catch2 for unit testing. Tests are built as part of the default build process.

To run tests:

```bash
cd build
ctest
```

Or, by directly running the executable:

```bash
./cpp_refresh # (or cpp_refresh.exe on Windows)
```
