function(add_config_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE config::config)
endfunction()

add_config_example(01_basic_usage 01_basic_usage.cpp)
add_config_example(02_save_strategies 02_save_strategies.cpp)
add_config_example(03_get_strategies 03_get_strategies.cpp)
add_config_example(04_obfuscation 04_obfuscation.cpp)
add_config_example(05_path_strategies 05_path_strategies.cpp)
add_config_example(06_listeners 06_listeners.cpp)
add_config_example(07_global_functions 07_global_functions.cpp)
add_config_example(09_struct_serialization 09_struct_serialization.cpp)

