idf_component_register(SRCS "src/Status.cpp" "src/Types.cpp" INCLUDE_DIRS "include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
