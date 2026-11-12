// The board test console's wire format (#15): command lines, JSON replies, base64, paths.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "board_test/board_test.h"

TEST_CASE("command lines: only ours, words and quotes")
{
    std::vector<std::string> words;
    CHECK(board_test::ParseCommandLine("kst selftest\r\n", &words));
    CHECK(words == std::vector<std::string>{"selftest"});
    CHECK(board_test::ParseCommandLine("  kst press  down   click", &words));
    CHECK(words == std::vector<std::string>{"press", "down", "click"});
    CHECK(board_test::ParseCommandLine(R"(kst put /a.txt "hello world" "say \"hi\" \\ ok")", &words));
    CHECK(words == std::vector<std::string>{"put", "/a.txt", "hello world", "say \"hi\" \\ ok"});
    CHECK(board_test::ParseCommandLine("kst", &words));
    CHECK(words.empty());

    CHECK_FALSE(board_test::ParseCommandLine("I (123) Main: kst selftest", &words));
    CHECK_FALSE(board_test::ParseCommandLine("kstx selftest", &words));
    CHECK_FALSE(board_test::ParseCommandLine("kst put \"open", &words));
    CHECK_FALSE(board_test::ParseCommandLine("", &words));
}

TEST_CASE("JSON replies are escaped and ordered")
{
    const std::string json = board_test::JsonWriter()
                                 .Add("cmd", "selftest")
                                 .Add("ok", true)
                                 .Add("count", 3)
                                 .Add("temp", 21.5)
                                 .Add("nan", 0.0 / 0.0)
                                 .Add("text", "a \"quote\"\n\x01")
                                 .AddRaw("list", "[1,2]")
                                 .Finish();
    CHECK(json == R"({"cmd":"selftest","ok":true,"count":3,"temp":21.5,"nan":null,)"
                  R"("text":"a \"quote\"\n\u0001","list":[1,2]})");
    CHECK(board_test::JsonWriter().Finish() == "{}");
}

TEST_CASE("base64 round-trips and rejects junk")
{
    for (size_t size = 0; size < 10; ++size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i * 37 + 200);
        }
        const std::string text = board_test::Base64Encode(data.data(), data.size());
        std::vector<uint8_t> back;
        REQUIRE(board_test::Base64Decode(text, &back));
        CHECK(back == data);
    }
    const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o'};
    CHECK(board_test::Base64Encode(hello, 5) == "aGVsbG8=");
    std::vector<uint8_t> out;
    CHECK_FALSE(board_test::Base64Decode("abc", &out));
    CHECK_FALSE(board_test::Base64Decode("ab!d", &out));
    CHECK_FALSE(board_test::Base64Decode("a=bc", &out));
    CHECK_FALSE(board_test::Base64Decode("ab=c", &out));
}

TEST_CASE("only sensible SD card paths are allowed")
{
    CHECK(board_test::IsSafeCardPath("/.keepsake/provision.json"));
    CHECK(board_test::IsSafeCardPath("/books/Emma.epub"));
    CHECK(board_test::IsSafeCardPath("/"));
    CHECK_FALSE(board_test::IsSafeCardPath("books/x"));
    CHECK_FALSE(board_test::IsSafeCardPath("/books/../../etc"));
    CHECK_FALSE(board_test::IsSafeCardPath("/.."));
    CHECK_FALSE(board_test::IsSafeCardPath("//x"));
    CHECK_FALSE(board_test::IsSafeCardPath("/a\\b"));
    CHECK_FALSE(board_test::IsSafeCardPath("/caf\xC3\xA9"));
    CHECK_FALSE(board_test::IsSafeCardPath(""));
    CHECK_FALSE(board_test::IsSafeCardPath("/" + std::string(200, 'a')));
    CHECK(board_test::IsSafeCardPath("/a..b/c"));  // ".." only matters as a whole component
}
