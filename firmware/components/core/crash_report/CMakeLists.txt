# Crash reports (#41): the log ring that survives a reset and the report text. Portable
# (host-tested and fuzzed); crash_report_service does the ESP-IDF and SD card side.
idf_component_register(
    SRCS "crash_report.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
