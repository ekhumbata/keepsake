# The provisioning file (#42): a small JSON reader and the validation of /.keepsake/provision.json.
# Portable (host-tested and fuzzed); provisioning_service does the SD card and NVS side.
idf_component_register(
    SRCS "mini_json.cpp" "provision.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
