# Portable page layout for the reader: blocks (epub_engine/content.h) -> lines -> pages.
idf_component_register(
    SRCS "book_layout.cpp"
    INCLUDE_DIRS "include"
    REQUIRES epub_engine
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
