# Battery UX (#84): the smoothed percentage, the low-battery nudges and their words. Portable
# (host-tested and fuzzed); firmware/main/battery_runtime.cpp watches the PMIC and shows them.
idf_component_register(
    SRCS "battery_ux.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
