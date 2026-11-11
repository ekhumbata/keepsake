idf_component_register(
    SRCS "cpu_boost.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_pm esp_hw_support
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
