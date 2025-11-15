add_executable(basic_get_set main.cpp)
target_link_libraries(basic_get_set PRIVATE config)
set_target_properties(basic_get_set PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)