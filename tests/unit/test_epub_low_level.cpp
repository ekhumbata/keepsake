// Low-level EPUB engine pieces: inflate, CRC, hrefs, the XML tokenizer, the zip reader.
#include <doctest/doctest.h>

#include <string>

#include "epub_engine/byte_source.h"
#include "epub_engine/href.h"
#include "epub_engine/inflate.h"
#include "epub_engine/xml_tokenizer.h"
#include "epub_engine/zip_archive.h"

using epub::InflateRaw;
using epub::InflateStatus;

namespace {

std::string Bytes(std::initializer_list<int> values)
{
    std::string out;
    for (int v : values) {
        out.push_back(static_cast<char>(v));
    }
    return out;
}

InflateStatus Inflate(const std::string& input, std::string* out, size_t max = 1 << 20)
{
    out->clear();
    return InflateRaw(reinterpret_cast<const uint8_t*>(input.data()), input.size(), max, out);
}

}  // namespace

TEST_CASE("CRC-32 matches the standard check value")
{
    CHECK(epub::Crc32("123456789", 9) == 0xCBF43926u);
    CHECK(epub::Crc32("", 0) == 0u);
    // Running CRC over two halves equals the CRC of the whole.
    CHECK(epub::Crc32("6789", 4, epub::Crc32("12345", 5)) == 0xCBF43926u);
}

TEST_CASE("inflate: stored, fixed and back-referencing blocks")
{
    std::string out;
    // Stored block, final: "hi".
    CHECK(Inflate(Bytes({0x01, 0x02, 0x00, 0xFD, 0xFF, 'h', 'i'}), &out) == InflateStatus::kOk);
    CHECK(out == "hi");

    // zlib.compressobj(wbits=-15).compress(b"a") (fixed Huffman).
    CHECK(Inflate(Bytes({0x4B, 0x04, 0x00}), &out) == InflateStatus::kOk);
    CHECK(out == "a");

    // Raw deflate of b"abcabcabcabcabc" (fixed Huffman with a length/distance match).
    CHECK(Inflate(Bytes({0x4B, 0x4C, 0x4A, 0x4E, 0x44, 0x42, 0x00}), &out) == InflateStatus::kOk);
    CHECK(out == "abcabcabcabcabc");
}

TEST_CASE("inflate rejects malformed streams and respects the output limit")
{
    std::string out;
    CHECK(Inflate("", &out) == InflateStatus::kCorrupt);
    CHECK(Inflate(Bytes({0x07}), &out) == InflateStatus::kCorrupt);  // reserved block type 3
    // Stored block whose LEN/NLEN don't match.
    CHECK(Inflate(Bytes({0x01, 0x02, 0x00, 0x00, 0x00, 'h', 'i'}), &out) == InflateStatus::kCorrupt);
    // Stored block claiming more bytes than present.
    CHECK(Inflate(Bytes({0x01, 0x05, 0x00, 0xFA, 0xFF, 'h'}), &out) == InflateStatus::kCorrupt);
    // Valid data, but more output than allowed: stops at the limit.
    CHECK(Inflate(Bytes({0x4B, 0x4C, 0x4A, 0x4E, 0x44, 0x42, 0x00}), &out, 5) ==
          InflateStatus::kTooLarge);
    CHECK(out.size() <= 5);
}

TEST_CASE("hrefs resolve relative to their document")
{
    CHECK(epub::DirectoryOf("OEBPS/text/one.xhtml") == "OEBPS/text/");
    CHECK(epub::DirectoryOf("one.xhtml").empty());

    std::string fragment;
    CHECK(epub::ResolveHref("OEBPS/text/", "../images/a.png") == "OEBPS/images/a.png");
    CHECK(epub::ResolveHref("OEBPS/", "./text/one.xhtml#part2", &fragment) == "OEBPS/text/one.xhtml");
    CHECK(fragment == "part2");
    CHECK(epub::ResolveHref("book/", "chapter%201.xhtml") == "book/chapter 1.xhtml");
    CHECK(epub::ResolveHref("a/b/", "/root.xhtml") == "root.xhtml");
    CHECK(epub::ResolveHref("", "../../../escape.xhtml") == "escape.xhtml");
    CHECK(epub::ResolveHref("OEBPS/", "https://example.com/x").empty());
    CHECK(epub::ResolveHref("OEBPS/", "mailto:someone").empty());
    CHECK(epub::ResolveHref("OEBPS/one.xhtml", "#note", &fragment) == "OEBPS/one.xhtml");
    CHECK(fragment == "note");
}

TEST_CASE("the XML tokenizer handles real-world EPUB markup")
{
    epub::XmlTokenizer tokens(
        "<?xml version='1.0'?><!DOCTYPE html [<!ENTITY x 'y'>]><!-- c -->"
        "<dc:Title id=t lang='en'>A &amp; B&nbsp;&#x2014;&#169;&bogus;</dc:Title>"
        "<br/><p>1 < 2</p><![CDATA[<raw>]]>");

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kStartTag);
    CHECK(tokens.name() == "dc:Title");
    CHECK(tokens.local_name() == "title");
    REQUIRE(tokens.Attribute("id") != nullptr);
    CHECK(*tokens.Attribute("id") == "t");
    CHECK(*tokens.Attribute("lang") == "en");

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kText);
    CHECK(tokens.text() == "A & B\xC2\xA0\xE2\x80\x94\xC2\xA9&bogus;");

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kEndTag);
    CHECK(tokens.local_name() == "title");

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kStartTag);
    CHECK(tokens.local_name() == "br");
    CHECK(tokens.self_closing());

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kStartTag);
    std::string text;
    while (tokens.Next() == epub::XmlTokenizer::Type::kText) {
        text += tokens.text();
    }
    CHECK(text == "1 < 2");
    CHECK(tokens.type() == epub::XmlTokenizer::Type::kEndTag);

    REQUIRE(tokens.Next() == epub::XmlTokenizer::Type::kText);
    CHECK(tokens.text() == "<raw>");
    CHECK(tokens.Next() == epub::XmlTokenizer::Type::kEnd);
}

TEST_CASE("the zip reader rejects things that aren't zips")
{
    epub::ZipArchive zip;
    epub::MemoryByteSource empty("");
    CHECK(zip.Open(&empty) == epub::ZipStatus::kNotZip);
    epub::MemoryByteSource text(std::string(1000, 'x'));
    CHECK(zip.Open(&text) == epub::ZipStatus::kNotZip);
    // An end record pointing outside the file.
    std::string eocd = Bytes({0x50, 0x4B, 0x05, 0x06, 0, 0, 0, 0, 1, 0, 1, 0, 46, 0, 0, 0, 0x10, 0, 0, 0, 0, 0});
    epub::MemoryByteSource bogus(eocd);
    CHECK(zip.Open(&bogus) == epub::ZipStatus::kCorrupt);
}
