# Exercise redirected stdin through the desktop executable and its C runtime.
string(ASCII 26 control_z)
set(input "${CMAKE_CURRENT_BINARY_DIR}/human-terminal-input.bin")
file(WRITE "${input}" "model SEN0592${control_z}\nmodel\n")
execute_process(COMMAND "${TERMINAL}" INPUT_FILE "${input}"
  RESULT_VARIABLE result OUTPUT_VARIABLE transcript TIMEOUT 5)
if(NOT result STREQUAL "0" OR NOT transcript MATCHES
    "\\[E\\] Invalid or oversized input line[\r\n]+> \\[I\\] Model unselected")
  message(FATAL_ERROR "The terminal lost the invalid line or the following query.\n${transcript}")
endif()

# EOF after admission still allows the exchange and its terminal output to finish.
file(WRITE "${input}" "model SEN0591\nread realtime\n")
execute_process(COMMAND "${TERMINAL}" INPUT_FILE "${input}"
  RESULT_VARIABLE result OUTPUT_VARIABLE transcript ERROR_VARIABLE identity TIMEOUT 5)
if(NOT result STREQUAL "0" OR NOT identity MATCHES "Desktop FC03/FC06 simulator"
    OR NOT transcript MATCHES "Operation 1 OK detail 0[^\r\n]*transfers 1"
    OR NOT transcript MATCHES "751 mm, raw 0x02EF")
  message(FATAL_ERROR "Simulated live read failed.\n${transcript}")
endif()

file(WRITE "${input}" "model SEN0592\nuseaddr 2\nread temperature\n")
execute_process(COMMAND "${TERMINAL}" INPUT_FILE "${input}"
  RESULT_VARIABLE result OUTPUT_VARIABLE transcript TIMEOUT 5)
if(NOT result STREQUAL "0" OR NOT transcript MATCHES
    "Operation 1 TIMEOUT detail 0[^\r\n]*address 2[^\r\n]*transfers 1, wire settlement required")
  message(FATAL_ERROR "The silent simulated address did not reach its deadline.\n${transcript}")
endif()

file(WRITE "${input}" "model SEN0591\nuseaddr 2\nwatch processed 2 200\njob status\nlatest processed\nstop 1\n")
execute_process(COMMAND "${TERMINAL}" INPUT_FILE "${input}"
  RESULT_VARIABLE result OUTPUT_VARIABLE transcript TIMEOUT 5)
if(NOT result STREQUAL "0" OR NOT transcript MATCHES "Operation 1 running, deadline"
    OR NOT transcript MATCHES "processed 0x0100, never sampled"
    OR NOT transcript MATCHES "Operation 1 ABORTED[^\r\n]*transfers 1, wire settlement required")
  message(FATAL_ERROR "The running watch did not service the query or stop request.\n${transcript}")
endif()
