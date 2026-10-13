cmake_minimum_required(VERSION 3.28)
project(config VERSION 0.2.0 LANGUAGES CXX)
include(cmake/DetectMsvcIncludes.cmake)

option(CONFIG_FETCH_JSON "Fetch nlohmann_json when no target or package is supplied" ON)
option(CONFIG_INSTALL "Generate install rules (off when embedded)" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_CONFIG_EXAMPLES "Build compiled usage examples" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_CONFIG_BENCHMARK "Build benchmarks" OFF)
option(BUILD_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(ENABLE_COVERAGE "Instrument project tests for coverage" OFF)
option(CONFIG_SANITIZER "Enable AddressSanitizer (and UBSan on Unix)" OFF)

if(NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json CONFIG QUIET)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json AND CONFIG_FETCH_JSON)
  include(FetchContent)
  set(JSON_Install ON CACHE BOOL "Install JSON with the standalone package")
  FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(json)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR "Provide nlohmann_json::nlohmann_json or enable CONFIG_FETCH_JSON")
endif()

find_package(Threads REQUIRED)
include(GNUInstallDirs)
add_library(config INTERFACE)
add_library(config::config ALIAS config)
target_compile_features(config INTERFACE cxx_std_20)
target_include_directories(config INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(config INTERFACE nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
  target_link_libraries(config INTERFACE Shell32)
endif()

function(config_enable_sanitizer target)
  if(CONFIG_SANITIZER)
    if(MSVC)
      target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
      target_link_options(${target} PRIVATE /INCREMENTAL:NO)
    else()
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# 开发选项仅作用于本项目目标，不修改宿主项目的编译标志。
function(config_development_target target)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  config_enable_sanitizer(${target})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(ENABLE_COVERAGE)
      target_compile_options(${target} PRIVATE -O0 -g --coverage)
      target_link_options(${target} PRIVATE --coverage)
    endif()
  endif()
endfunction()

include(CMakePackageConfigHelpers)
configure_file(include/config/version.h.in include/config/version.h @ONLY)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
configure_package_config_file(cmake/configConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/config")
if(CONFIG_INSTALL)
  install(TARGETS config EXPORT config-targets)
  install(DIRECTORY include/config DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/config/version.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/config")
  install(EXPORT config-targets NAMESPACE config:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/config")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/config")
  install(FILES LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/config")
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
if(BUILD_CONFIG_EXAMPLES)
  add_subdirectory(examples)
endif()
if(BUILD_CONFIG_BENCHMARK)
  add_subdirectory(benchmark)
endif()
