cmake_minimum_required(VERSION 3.28)

project(config VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/utf-8)
endif()

option(CONFIG_FETCH_JSON "Automatically fetch nlohmann_json if not found" ON)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND AND CONFIG_FETCH_JSON)
  include(FetchContent)
  FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
  FetchContent_MakeAvailable(json)
endif()

add_library(config INTERFACE)
add_library(config::config ALIAS config)

target_include_directories(config INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(config INTERFACE cxx_std_20)
target_link_libraries(config INTERFACE nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(config INTERFACE Shell32)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  TARGETS config
  EXPORT config-targets
)

install(
  DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)

install(
  EXPORT config-targets
  NAMESPACE config::
  DESTINATION lib/cmake/config
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/configConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake
  INSTALL_DESTINATION lib/cmake/config
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake
  DESTINATION lib/cmake/config
)

option(BUILD_CONFIG_EXAMPLES "Build the example executable for the config library" ON)
if(BUILD_CONFIG_EXAMPLES)
  add_subdirectory(examples)
endif()
