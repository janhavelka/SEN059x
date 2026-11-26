# Exercise redirected stdin through the desktop executable and its C runtime.
string(ASCII 26 control_z)
set(input "${CMAKE_CURRENT_BINARY_DIR}/human-terminal-input.bin")
file(WRITE "${input}" "model SEN0592${control_z}\nmodel\n")
execute_process(COMMAND "${TERMINAL}" INPUT_FILE "${input}"
  RESULT_VARIABLE result OUTPUT_VARIABLE transcript)
if(NOT result STREQUAL "0" OR NOT transcript MATCHES
    "\\[E\\] Invalid or oversized input line[\r\n]+> \\[I\\] Model unselected")
  message(FATAL_ERROR "The terminal lost the invalid line or the following query.\n${transcript}")
endif()
