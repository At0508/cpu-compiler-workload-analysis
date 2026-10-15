execute_process(
  COMMAND "${OPT_EXECUTABLE}" "-load-pass-plugin=${PLUGIN_FILE}"
          "-passes=function(loop-report)" -disable-output "${INPUT_IR}"
  RESULT_VARIABLE exit_code
  ERROR_VARIABLE report)
if(NOT exit_code EQUAL 0)
  message(FATAL_ERROR "opt failed (${exit_code}): ${report}")
endif()
if(NOT report MATCHES "LOOP_REPORT function=fixed_loop depth=1 blocks=1 instructions=[0-9]+ loads=1 stores=1 calls=0 constant_trip_count=16")
  message(FATAL_ERROR "fixed loop report mismatch: ${report}")
endif()
if(NOT report MATCHES "LOOP_REPORT function=dynamic_loop depth=1 blocks=1 instructions=[0-9]+ loads=1 stores=0 calls=0 constant_trip_count=unknown")
  message(FATAL_ERROR "dynamic loop report mismatch: ${report}")
endif()
message(STATUS "Loop report matched fixed and dynamic fixtures")
