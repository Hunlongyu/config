include(FetchContent)
if(NOT TARGET GTest::gtest_main)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "Unused by config tests" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "Do not install test dependencies" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0)
  FetchContent_MakeAvailable(googletest)
  config_enable_sanitizer(gtest)
  config_enable_sanitizer(gtest_main)
endif()

foreach(suite IN ITEMS store reads persistence sources subscriptions concurrency watcher)
  add_executable(test_${suite} test_${suite}.cpp)
  target_link_libraries(test_${suite} PRIVATE config::config GTest::gtest_main)
  config_development_target(test_${suite})
  add_test(NAME ${suite} COMMAND test_${suite})
  set_tests_properties(${suite} PROPERTIES TIMEOUT 60)
endforeach()

# 缺失保存串行锁的副本必须被确定性交错测试拒绝。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/config/detail/state.hpp")
file(READ "${PROJECT_SOURCE_DIR}/include/config/detail/state.hpp" mutant)
string(REPLACE "std::lock_guard writer_lock(writer_);\n        const auto before = snapshot();"
  "/* mutation: save serialization removed */\n        const auto before = snapshot();" mutant "${mutant}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mutant/config/detail")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mutant/config/detail/state.hpp" "${mutant}")
add_executable(test_save_mutant test_concurrency.cpp)
target_include_directories(test_save_mutant BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/mutant")
target_link_libraries(test_save_mutant PRIVATE config::config GTest::gtest_main)
config_development_target(test_save_mutant)
add_test(NAME rejects_missing_save_lock COMMAND test_save_mutant
  --gtest_filter=Concurrency.SaveSerializesWithMutations)
set_tests_properties(rejects_missing_save_lock PROPERTIES WILL_FAIL TRUE TIMEOUT 30)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME documentation COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_docs.py")
endif()
if(WIN32)
  add_executable(test_unicode test_sources.cpp)
  target_compile_definitions(test_unicode PRIVATE UNICODE _UNICODE)
  target_link_libraries(test_unicode PRIVATE config::config GTest::gtest_main)
  config_development_target(test_unicode)
  add_test(NAME unicode COMMAND test_unicode)
endif()

# 每个公共头分别成为独立翻译单元，不依赖包含顺序。
file(GLOB public_headers "${PROJECT_SOURCE_DIR}/include/config/*.hpp")
set(header_units "")
foreach(header IN LISTS public_headers)
  get_filename_component(name "${header}" NAME)
  string(MAKE_C_IDENTIFIER "${name}" symbol)
  set(unit "${CMAKE_CURRENT_BINARY_DIR}/header_${symbol}.cpp")
  file(WRITE "${unit}" "#include <config/${name}>\n#include <algorithm>\nint header_${symbol}() { return std::max(1,2); }\n")
  list(APPEND header_units "${unit}")
endforeach()
add_executable(test_headers consumer_main.cpp consumer_peer.cpp ${header_units})
target_link_libraries(test_headers PRIVATE config::config)
target_compile_definitions(test_headers PRIVATE CONFIG_TEST_VERSION="${PROJECT_VERSION}")
config_development_target(test_headers)
add_test(NAME headers_and_multiple_translation_units COMMAND test_headers)

foreach(mode IN ITEMS install parent manual)
  add_test(NAME consumer_${mode}
    COMMAND "${CMAKE_COMMAND}"
      "-DMODE=${mode}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBINARY_DIR=${PROJECT_BINARY_DIR}" "-DJSON_SOURCE_DIR=${json_SOURCE_DIR}"
      "-DCOMPILER=${CMAKE_CXX_COMPILER}" "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}" "-DCONFIG=$<CONFIG>"
      "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/test_consumer.cmake")
  set_tests_properties(consumer_${mode} PROPERTIES TIMEOUT 180)
endforeach()
