cmake_minimum_required(VERSION 3.16)
project(SEN059xNativeTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(sen059x STATIC ../../src/ReadCodec.cpp ../../src/Status.cpp ../../src/Types.cpp)
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
