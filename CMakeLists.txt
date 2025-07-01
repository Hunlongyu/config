cmake_minimum_required(VERSION 3.28)

project(config VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/utf-8)
endif()

if(WIN32)
  link_libraries(version.lib)
endif()

include(FetchContent)

FetchContent_Declare(json URL https://gh.nxnow.top/https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# 主库（仅头文件库）
add_library(config INTERFACE)

target_include_directories(config INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(config INTERFACE nlohmann_json::nlohmann_json)

install(
  DIRECTORY include/
  DESTINATION include
  PATTERN "*.h"
)

# 可选示例
option(BUILD_CONFIG_EXAMPLES "Build the example executable for the config library" ON)

if(BUILD_CONFIG_EXAMPLES)
  add_subdirectory(examples)
endif()
