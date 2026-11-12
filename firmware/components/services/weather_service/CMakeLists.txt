# Weather (#37): an hourly Open-Meteo fetch as a background network job, cached in NVS. The
# parsing and rules live in core/weather (host-tested).
idf_component_register(
    SRCS "weather_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES weather
    PRIV_REQUIRES esp_http_client esp_timer mbedtls net_jobs_service nvs_flash
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
