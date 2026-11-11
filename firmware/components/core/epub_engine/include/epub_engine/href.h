#ifndef EPUB_ENGINE_HREF_H_
#define EPUB_ENGINE_HREF_H_

#include <string>
#include <string_view>

namespace epub {

// Directory part of a zip path, with a trailing slash: "OEBPS/text/ch1.xhtml" ->
// "OEBPS/text/". Empty for a file at the root.
std::string DirectoryOf(std::string_view path);

// Resolves an href found in a document at `base_directory` to a normalized zip entry path:
// percent-decodes it, drops the "#fragment" (returned through *fragment when non-null),
// applies "." and ".." segments, and treats a leading "/" as the zip root. A ".." that would
// climb above the root is dropped. External links (with a URL scheme) return "".
std::string ResolveHref(std::string_view base_directory, std::string_view href,
                        std::string* fragment = nullptr);

}  // namespace epub

#endif  // EPUB_ENGINE_HREF_H_
