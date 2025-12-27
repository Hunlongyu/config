cmake_minimum_required(VERSION 3.28)

project(
  config
  VERSION 0.1.2
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Clang-Tidy Support
option(BUILD_CLANG_TIDY "Enable Clang-Tidy static analysis" OFF)
if(BUILD_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    message(STATUS "Clang-Tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  else()
    message(WARNING "Clang-Tidy requested but not found!")
  endif()
endif()

if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage enabled")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -O0 -g --coverage -fprofile-update=atomic")
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -O0 -g --coverage -fprofile-update=atomic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
  endif()
endif()

option(CONFIG_FETCH_JSON "Automatically fetch nlohmann_json if not found" ON)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND AND CONFIG_FETCH_JSON)
  include(FetchContent)
  FetchContent_Declare(
    json
    GIT_REPOSITORY https://gitee.com/mirrors/nlohmann-json.git
    GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(json)
  set(CONFIG_OWNS_JSON ON)
  if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  endif()
endif()

add_library(config INTERFACE)
add_library(config::config ALIAS config)

target_include_directories(
  config
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include>)

target_compile_features(config INTERFACE cxx_std_20)
target_link_libraries(config INTERFACE nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(config INTERFACE Shell32)
endif()

include(CMakePackageConfigHelpers)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config/version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/config/version.h @ONLY)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(TARGETS config EXPORT config-targets)

if(CONFIG_OWNS_JSON)
  install(TARGETS nlohmann_json EXPORT config-targets)
endif()

install(
  DIRECTORY include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.hpp")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/config/version.h
        DESTINATION include/config)

install(
  EXPORT config-targets
  NAMESPACE config::
  DESTINATION lib/cmake/config)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/configConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake
  INSTALL_DESTINATION lib/cmake/config)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/configConfigVersion.cmake
        DESTINATION lib/cmake/config)

option(BUILD_CONFIG_EXAMPLES
       "Build the example executable for the config library"
       ${PROJECT_IS_TOP_LEVEL})
if(BUILD_CONFIG_EXAMPLES)
  add_subdirectory(examples)
endif()

option(BUILD_CONFIG_BENCHMARK
       "Build the benchmark executable for the config library"
       ${PROJECT_IS_TOP_LEVEL})
if(BUILD_CONFIG_BENCHMARK)
  add_subdirectory(benchmark)
endif()

option(BUILD_TESTING "Build the testing tree." ${PROJECT_IS_TOP_LEVEL})
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
