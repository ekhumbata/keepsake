#ifndef EPUB_ENGINE_XHTML_PARSER_H_
#define EPUB_ENGINE_XHTML_PARSER_H_

#include <cstddef>
#include <string_view>

#include "epub_engine/content.h"

namespace epub {

struct XhtmlLimits {
    size_t max_blocks = 20000;
    size_t max_text_bytes = 4u * 1024u * 1024u;
};

// Turns one XHTML content document into blocks. `document_path` is the document's zip path
// (used to resolve image hrefs). Handles paragraphs, headings, lists, blockquotes, <pre>,
// <br>, <hr>, images (as placeholders) and bold/italic inline styles; skips <head>,
// <script>, <style> and SVG drawing content. Whitespace is collapsed the way a browser does.
// Never fails: malformed markup produces whatever text could be recovered.
Chapter ParseXhtml(std::string_view xhtml, std::string_view document_path,
                   const XhtmlLimits& limits = {});

}  // namespace epub

#endif  // EPUB_ENGINE_XHTML_PARSER_H_
