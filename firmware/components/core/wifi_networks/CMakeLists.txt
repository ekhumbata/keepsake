# Saved Wi-Fi networks (#43): the list, which one to try next, and how it's stored. Portable
# (host-tested and fuzzed); wifi_service keeps it in NVS and does the connecting.
idf_component_register(
    SRCS "wifi_networks.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
