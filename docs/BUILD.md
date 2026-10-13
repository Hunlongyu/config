# Build and integration

[中文](BUILD.zh-CN.md) · [Home](../README.md)

Requires a C++20 compiler, CMake 3.28+ for CMake integration, and nlohmann/json. The fetched JSON revision is 3.11.3. No Qt dependency.

## Build and test
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --parallel
```
On Windows use a Visual Studio developer shell first. Local discovery supports Visual Studio preview through vswhere -prerelease. Windows presets windows-debug/windows-release include install rules and consumer tests.

BUILD_TESTING and BUILD_CONFIG_EXAMPLES default on for standalone builds. BUILD_CONFIG_BENCHMARK defaults off and fetches Google Benchmark when enabled. CONFIG_SANITIZER enables MSVC ASan or Unix ASan/UBSan; use RelWithDebInfo for MSVC ASan. ENABLE_COVERAGE enables Unix coverage instrumentation; GCC uses atomic profile counters for concurrent tests. Python 3, when available, adds the documentation synchronization test; CI checks it explicitly.

## Parent projects and offline use

Link config::config. Dependency resolution first reuses nlohmann_json::nlohmann_json, then searches for a package, then fetches if CONFIG_FETCH_JSON is enabled. Set CONFIG_FETCH_JSON=OFF to require an existing target/package.

For an offline checkout, pass FETCHCONTENT_SOURCE_DIR_JSON and FETCHCONTENT_SOURCE_DIR_GOOGLETEST. The optional benchmark uses FETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK. Development warnings/instrumentation apply only to this project's executable targets.

CONFIG_INSTALL defaults on for a standalone build and off when embedded. An embedded project enabling installation must also provide an installable JSON dependency. This avoids imposing export rules on a parent's privately fetched JSON target.

## Installed package
```sh
cmake -S . -B package -DBUILD_TESTING=OFF -DBUILD_CONFIG_EXAMPLES=OFF
cmake --install package --prefix install
```
The default fetched dependency is installed with the library; the prefix includes public/detail headers, generated version.h, CMake package files, JSON, and both licenses under share/licenses/. With an externally supplied JSON package, consumers must also be able to find that package.
```cmake
find_package(config 0.2 CONFIG REQUIRED)
target_link_libraries(app PRIVATE config::config)
```
Package compatibility is limited to the same minor version because 0.2 changes the API.

## Manual copying

Copy include/config and provide nlohmann/json headers. Enable C++20 and platform thread support (for example -pthread on toolchains that require it). On Windows, link Shell32.lib if using user_config_directory. config.hpp does not require the generated version header; copy generated version.h separately only if the application uses version().

The consumer tests build the library as an installed package, as a parent subdirectory with an already supplied dependency, and by directly including its headers. Independent-header and multiple-translation-unit tests catch macro leakage and missing inline definitions.
