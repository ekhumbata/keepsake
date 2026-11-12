# Personal touches (#85): the Home greeting with the friend's name and the birthday. Portable,
# host-tested; the values come from the device (provisioning file, #42), never the repo.
idf_component_register(
    SRCS "greeting.cpp" "special_days.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
