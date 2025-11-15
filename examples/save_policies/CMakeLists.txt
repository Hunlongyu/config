add_executable(save_policies main.cpp)
target_link_libraries(save_policies PRIVATE config)
set_target_properties(save_policies PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)