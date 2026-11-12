# The ambient lock screen (#38): night hours, the clock text and the month grid. Portable
# (host-tested); epaper_ui draws the screen and lock_screen_runtime fills it.
idf_component_register(
    SRCS "ambient.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
