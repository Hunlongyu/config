add_executable(save_formats main.cpp)
target_link_libraries(save_formats PRIVATE config)
set_target_properties(save_formats PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)