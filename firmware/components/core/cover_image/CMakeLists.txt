# Book covers (#27): decode an EPUB's cover image, fit it to a thumbnail box and dither it to
# 1 bit, plus the small cache file format. Portable; JPEG/PNG/GIF decoding is stb_image
# (third_party/, public domain / MIT), compiled with warnings off since CI fails on any.
idf_component_register(
    SRCS "cover_image.cpp" "stb_image_impl.cpp"
    INCLUDE_DIRS "include"
)

set_source_files_properties(stb_image_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
