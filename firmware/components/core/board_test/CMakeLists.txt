# The board test console's wire format (#15, board tests): command lines in, framed JSON out.
# Portable and host-tested; firmware/main/test_console_runtime.cpp runs the commands.
idf_component_register(
    SRCS "board_test.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
