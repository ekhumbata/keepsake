# Updates over the air from GitHub Releases (#40): the daily check, the download into the other
# app slot, the health check after an update and the rollback. The decisions live in
# core/firmware_update (host-tested).
idf_component_register(
    SRCS "ota_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES firmware_update
    PRIV_REQUIRES app_update esp_app_format esp_http_client esp_partition esp_system esp_timer
                  mbedtls net_jobs_service nvs_flash power_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
