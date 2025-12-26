# Build and Install Guide

This guide provides instructions on how to build the library examples, run tests, and integrate the library into your project.

## Prerequisites

*   **C++ Compiler**: Must support **C++20** (e.g., GCC 10+, Clang 10+, MSVC 19.29+).
*   **CMake**: Version **3.28** or higher.
*   **Build System**: Ninja, Make, or Visual Studio (depending on your OS).

## Building from Source

If you want to run the examples or tests, you need to build the project from source.

### 1. Clone the Repository

```bash
git clone https://github.com/Hunlongyu/config.git
cd config
```

### 2. Configure the Project

Create a build directory and run CMake configuration:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

**Options:**
*   `-DBUILD_CONFIG_EXAMPLES=ON`: Build example programs (Default: ON).
*   `-DBUILD_TESTING=ON`: Build unit tests (Default: ON).
*   `-DBUILD_CONFIG_BENCHMARK=ON`: Build benchmark tool (Default: ON).

### 3. Build

```bash
cmake --build build --config Release
```

## Running Examples

After building, the example executables will be located in `build/examples/`.

```bash
# Example: Run the basic usage example
./build/examples/01_basic_usage
```

## Running Tests

To verify the library works correctly on your system, run the test suite:

```bash
cd build
ctest -C Release --output-on-failure
```

## Installation

Since this is a header-only library, "installation" simply means copying the headers to a standard location.

```bash
cmake --install build --prefix /usr/local
```

This will copy the headers to `/usr/local/include/config/`.

## Integration

### Using `find_package` (after installation)

If you have installed the library globally or locally, you can use `find_package` in your `CMakeLists.txt`:

```cmake
find_package(config REQUIRED)
target_link_libraries(your_target PRIVATE config::config)
```
