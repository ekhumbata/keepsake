idf_component_register(
    SRCS "climate_service.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board shtc3 esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
