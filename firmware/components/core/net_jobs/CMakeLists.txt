# Portable Wi-Fi-on-demand policy (#17): when to bring the radio up, run queued network
# jobs, linger, power down, and back off. No ESP-IDF; services/net_jobs drives wifi_service.
idf_component_register(
    SRCS "scheduler.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
