cmake_minimum_required(VERSION 3.28)

project(benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 config 库 假设 config 库在上一级目录构建，或者已安装 这里我们直接复用主项目的 target，因为 benchmark
# 是作为子目录或者单独构建的

if(NOT TARGET config::config)
  # 如果作为独立项目运行，需要引入 config 这里假设是在主项目的子目录中，直接链接 config::config
  message(STATUS "Config target found")
endif()

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE config::config)
