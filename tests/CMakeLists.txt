include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
  GIT_TAG v1.14.0)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

function(add_config_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE config::config GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_config_test(test_core test_core.cpp)
add_config_test(test_persistence test_persistence.cpp)
add_config_test(test_obfuscation test_obfuscation.cpp)
add_config_test(test_advanced test_advanced.cpp)
add_config_test(test_global_api test_global_api.cpp)
