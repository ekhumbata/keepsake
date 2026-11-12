# Photos (#136): the /photos folder on the SD card, the prepared-photo cache, and decoding on
# the device (TJpgDec from ROM for big JPEGs, core/photo for the rest).
idf_component_register(
    SRCS "photo_service.cpp" "tjpgd_decode.cpp"
    INCLUDE_DIRS "include"
    REQUIRES cover_image photo
    PRIV_REQUIRES esp_rom freertos nvs_flash storage_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
