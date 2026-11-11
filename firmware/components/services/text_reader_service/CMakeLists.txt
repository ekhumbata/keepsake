idf_component_register(
    SRCS "text_reader_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES storage_service
    PRIV_REQUIRES epub_engine library
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
