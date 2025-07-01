add_executable(example4 main.cpp)

# 链接 config 库
target_link_libraries(example4 PRIVATE config)

# 设置可执行文件输出目录
set_target_properties(example4 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)