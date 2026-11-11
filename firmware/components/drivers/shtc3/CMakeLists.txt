idf_component_register(
    SRCS "shtc3.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
    PRIV_REQUIRES esp_rom freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
