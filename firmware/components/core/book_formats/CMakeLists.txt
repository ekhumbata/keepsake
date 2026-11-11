# Portable file-format sniffing and the friendly messages for books that can't be opened (#29).
idf_component_register(
    SRCS "book_formats.cpp"
    INCLUDE_DIRS "include"
    REQUIRES epub_engine
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
