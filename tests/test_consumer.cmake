function(run)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Command failed: ${ARGV}\n${output}\n${error}")
  endif()
endfunction()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(work "${BINARY_DIR}/consumer-${MODE}/${run_id}")
set(prefix "${work}/prefix")
set(generator_args -G "${GENERATOR}")
if(PLATFORM)
  list(APPEND generator_args -A "${PLATFORM}")
endif()
if(TOOLSET)
  list(APPEND generator_args -T "${TOOLSET}")
endif()
set(common ${generator_args}
  "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
  "-DCMAKE_BUILD_TYPE=${CONFIG}" -DCMAKE_CXX_SCAN_FOR_MODULES=OFF)
set(dependency_args)
if(JSON_SOURCE_DIR)
  list(APPEND dependency_args "-DFETCHCONTENT_SOURCE_DIR_JSON=${JSON_SOURCE_DIR}")
endif()
if(MODE STREQUAL "install")
  run("${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${work}/library" ${common}
    -DBUILD_TESTING=OFF -DBUILD_CONFIG_EXAMPLES=OFF -DBUILD_CONFIG_BENCHMARK=OFF
    -DCONFIG_FETCH_JSON=ON -DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=TRUE ${dependency_args})
  run("${CMAKE_COMMAND}" --install "${work}/library" --prefix "${prefix}" --config "${CONFIG}")
  foreach(license IN ITEMS config/LICENSE.txt nlohmann_json/LICENSE.MIT)
    if(NOT EXISTS "${prefix}/share/licenses/${license}")
      message(FATAL_ERROR "Installed package is missing ${license}")
    endif()
  endforeach()
endif()
run("${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/consumer" -B "${work}/application" ${common}
  "-DMODE=${MODE}" "-DCONFIG_SOURCE_DIR=${SOURCE_DIR}"
  "-DCONFIG_TEST_PREFIX=${prefix}" "-DCMAKE_PREFIX_PATH=${prefix}"
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
  ${dependency_args})
run("${CMAKE_COMMAND}" --build "${work}/application" --config "${CONFIG}")
run("${CTEST_COMMAND}" --test-dir "${work}/application" -C "${CONFIG}" --output-on-failure)
