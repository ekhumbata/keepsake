# Updates over the air (#40): versions, the release manifest, the health check and the
# plain-language status. Portable (host-tested and fuzzed); ota_service does the download,
# the flash writes and the rollback.
idf_component_register(
    SRCS "firmware_update.cpp"
    INCLUDE_DIRS "include"
    REQUIRES provisioning
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
