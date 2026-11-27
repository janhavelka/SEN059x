cmake_minimum_required(VERSION 3.16)
project(SEN059xNativeTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(sen059x STATIC
  ../../src/Configuration.cpp
  ../../src/DeviceHealth.cpp
  ../../src/MeasurementCache.cpp
  ../../src/Measurements.cpp
  ../../src/MeasurementTiming.cpp
  ../../src/ReadCodec.cpp
  ../../src/SettingDescriptors.cpp
  ../../src/Settings.cpp
  ../../src/Stats.cpp
  ../../src/Status.cpp
  ../../src/Types.cpp
  ../../src/WriteCodec.cpp
)
target_include_directories(sen059x PUBLIC ../../include)
target_compile_features(sen059x PUBLIC cxx_std_17)

add_executable(value_status
  test_value_status.cpp
  header_status.cpp
  header_types.cpp
  header_umbrella.cpp
  header_version.cpp
)
target_link_libraries(value_status PRIVATE sen059x)
add_test(NAME value_status COMMAND value_status)

add_executable(read_codec test_read_codec.cpp)
target_link_libraries(read_codec PRIVATE sen059x)
add_test(NAME read_codec COMMAND read_codec)

add_executable(write_codec test_write_codec.cpp)
target_link_libraries(write_codec PRIVATE sen059x)
add_test(NAME write_codec COMMAND write_codec)

add_executable(measurements test_measurements.cpp)
target_link_libraries(measurements PRIVATE sen059x)
add_test(NAME measurements COMMAND measurements)

add_executable(measurement_timing test_measurement_timing.cpp)
target_link_libraries(measurement_timing PRIVATE sen059x)
add_test(NAME measurement_timing COMMAND measurement_timing)

add_executable(measurement_cache test_measurement_cache.cpp)
target_link_libraries(measurement_cache PRIVATE sen059x)
add_test(NAME measurement_cache COMMAND measurement_cache)

add_executable(measurement_binding test_measurement_binding.cpp)
target_link_libraries(measurement_binding PRIVATE sen059x)
add_test(NAME measurement_binding COMMAND measurement_binding)

add_executable(settings test_settings.cpp)
target_link_libraries(settings PRIVATE sen059x)
add_test(NAME settings COMMAND settings)

add_executable(configuration test_configuration.cpp)
target_link_libraries(configuration PRIVATE sen059x)
add_test(NAME configuration COMMAND configuration)

add_executable(device_health test_device_health.cpp)
target_link_libraries(device_health PRIVATE sen059x)
add_test(NAME device_health COMMAND device_health)

add_executable(stats test_stats.cpp)
target_link_libraries(stats PRIVATE sen059x)
add_test(NAME stats COMMAND stats)

add_executable(exchange_timing test_exchange_timing.cpp)
target_link_libraries(exchange_timing PRIVATE sen059x)
add_test(NAME exchange_timing COMMAND exchange_timing)

add_library(owner_scheduler STATIC
  ../../examples/common/Exchange.cpp
  ../../examples/common/Scheduler.cpp
)
target_include_directories(owner_scheduler PUBLIC ../../examples/common)
target_link_libraries(owner_scheduler PUBLIC sen059x)

add_library(human_console STATIC
  ../../examples/common/HumanConsole.cpp
  ../../examples/common/HumanCommands.cpp
  ../../examples/common/HumanJobs.cpp
  ../../examples/common/HumanReports.cpp
  ../../examples/common/HumanSelftest.cpp
)
target_link_libraries(human_console PUBLIC owner_scheduler)

add_executable(test_human_console test_human_console.cpp)
target_link_libraries(test_human_console PRIVATE human_console)
add_test(NAME human_console COMMAND test_human_console)

foreach(suite human_driver human_reports human_live)
  add_executable(test_${suite} test_${suite}.cpp)
  target_link_libraries(test_${suite} PRIVATE human_console)
  add_test(NAME ${suite} COMMAND test_${suite})
endforeach()

find_package(Threads REQUIRED)
add_executable(human_terminal human_terminal.cpp)
target_link_libraries(human_terminal PRIVATE human_console Threads::Threads)
add_test(NAME human_terminal_input COMMAND ${CMAKE_COMMAND}
  -DTERMINAL=$<TARGET_FILE:human_terminal> -P ${CMAKE_CURRENT_SOURCE_DIR}/test_terminal_input.cmake)

foreach(suite exchange scheduler scheduler_raw scheduler_recovery scheduler_settings scheduler_commissioning scheduler_vendor)
  add_executable(${suite} test_${suite}.cpp)
  target_link_libraries(${suite} PRIVATE owner_scheduler)
  add_test(NAME ${suite} COMMAND ${suite})
endforeach()

add_library(fieldcore_bindings STATIC FieldCoreMainBinding.cpp FieldCoreRodtesterBinding.cpp)
target_link_libraries(fieldcore_bindings PUBLIC sen059x)

add_executable(fieldcore_main
  test_fieldcore_main.cpp
  test_fieldcore_main_owner.cpp
)
target_link_libraries(fieldcore_main PRIVATE fieldcore_bindings)
add_test(NAME fieldcore_main COMMAND fieldcore_main)

foreach(suite fieldcore_rodtester fieldcore_lifecycle fieldcore_administrative fieldcore_configuration)
  add_executable(${suite} test_${suite}.cpp)
  target_link_libraries(${suite} PRIVATE fieldcore_bindings)
  add_test(NAME ${suite} COMMAND ${suite})
endforeach()
