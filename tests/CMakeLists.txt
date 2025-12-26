include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

function(add_config_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE config::config GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_config_test(test_basic test_basic.cpp)
add_config_test(test_save_strategy test_save_strategy.cpp)
add_config_test(test_get_strategy test_get_strategy.cpp)
add_config_test(test_obfuscation test_obfuscation.cpp)
add_config_test(test_path test_path.cpp)
add_config_test(test_listener test_listener.cpp)
add_config_test(test_thread_safety test_thread_safety.cpp)
