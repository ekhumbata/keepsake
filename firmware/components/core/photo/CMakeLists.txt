# Photos (#136): EXIF orientation and date, preparing a photo for the 1-bit panel (orient, fit,
# auto-levels, dither), captions and order from file names, and the cache. Portable
# (host-tested and fuzzed); photo_service reads the SD card and decodes big JPEGs on the device.
idf_component_register(
    SRCS "photo.cpp"
    INCLUDE_DIRS "include"
    REQUIRES cover_image
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
