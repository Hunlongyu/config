# 某些中文 MSVC 输出 UTF-8，而 CMake 的默认探测按系统代码页解码。
# 用真实编译器输出重新确认前缀，保证 Ninja 跟踪头文件依赖。
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/config-include-probe")
  file(MAKE_DIRECTORY "${probe_dir}")
  file(WRITE "${probe_dir}/config_include_probe.hpp" "#pragma once\n")
  file(WRITE "${probe_dir}/probe.cpp" "#include \"config_include_probe.hpp\"\nint config_include_probe;\n")
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c /TP
      "${probe_dir}/probe.cpp" "/Fo${probe_dir}/probe.obj"
    RESULT_VARIABLE probe_result OUTPUT_VARIABLE probe_output ERROR_VARIABLE probe_error
    ENCODING UTF-8)
  file(TO_NATIVE_PATH "${probe_dir}/config_include_probe.hpp" probe_header)
  string(FIND "${probe_output}" "${probe_header}" header_offset)
  if(probe_result EQUAL 0 AND header_offset GREATER_EQUAL 0)
    string(SUBSTRING "${probe_output}" 0 ${header_offset} prefix_output)
    string(REGEX MATCH "[^\r\n]+$" actual_prefix "${prefix_output}")
    if(actual_prefix AND NOT actual_prefix MATCHES "�")
      set(CMAKE_CL_SHOWINCLUDES_PREFIX "${actual_prefix}")
    endif()
  endif()
endif()
