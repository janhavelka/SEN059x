idf_component_register(
  SRCS "src/ReadCodec.cpp" "src/Status.cpp" "src/Types.cpp" "src/WriteCodec.cpp"
  INCLUDE_DIRS "include"
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
