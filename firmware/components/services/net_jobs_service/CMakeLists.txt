idf_component_register(
    SRCS "net_jobs_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES net_jobs
    PRIV_REQUIRES esp_timer freertos task_config wifi_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
