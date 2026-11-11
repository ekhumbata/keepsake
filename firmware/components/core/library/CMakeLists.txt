# Portable library logic (#27): titles from file names, the cached index format, shelf order.
idf_component_register(
    SRCS "library.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
