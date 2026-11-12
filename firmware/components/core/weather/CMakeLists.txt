# Weather (#37): Open-Meteo URLs, response parsing, weather codes in words, the cache and the
# "updated X h ago" rule. Portable (host-tested and fuzzed); weather_service fetches.
idf_component_register(
    SRCS "weather.cpp"
    INCLUDE_DIRS "include"
    REQUIRES provisioning
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
