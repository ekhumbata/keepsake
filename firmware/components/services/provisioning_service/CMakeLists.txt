idf_component_register(
    SRCS "provisioning_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES provisioning
    PRIV_REQUIRES app_state_service gemini_service greeting nvs_flash storage_service timezone_service
                  wifi_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
