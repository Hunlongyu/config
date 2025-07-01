add_executable(example1 main.cpp)

# 链接 config 库
target_link_libraries(example1 PRIVATE config)

# 设置可执行文件输出目录
set_target_properties(example1 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)