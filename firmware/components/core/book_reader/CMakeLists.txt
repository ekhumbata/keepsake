# Portable reading session for EPUBs: book + current chapter's pages + position (#28).
idf_component_register(
    SRCS "epub_session.cpp"
    INCLUDE_DIRS "include"
    REQUIRES book_layout epub_engine
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
