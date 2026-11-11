# Portable EPUB engine (no ESP-IDF includes): zip + inflate, a forgiving XML tokenizer,
# OPF/NCX/nav parsing and XHTML -> block model. Host-tested and fuzzed under tests/.
idf_component_register(
    SRCS "byte_source.cpp" "inflate.cpp" "zip_archive.cpp" "xml_tokenizer.cpp" "href.cpp"
         "epub_book.cpp" "xhtml_parser.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
