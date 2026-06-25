cmake_minimum_required(VERSION 3.28)

include(FetchContent)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3)
set(BENCHMARK_ENABLE_TESTING
    OFF
    CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(config_benchmark benchmark.cpp)
target_link_libraries(config_benchmark PRIVATE config::config
                                               benchmark::benchmark)
