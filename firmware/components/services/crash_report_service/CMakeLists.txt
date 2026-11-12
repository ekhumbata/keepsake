idf_component_register(
    SRCS "crash_report_service.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES crash_report esp_app_format espcoredump esp_partition esp_system log
                  storage_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
