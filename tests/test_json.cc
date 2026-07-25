// JSON parsing, via nlohmann/json.
//
// Replaces the original RapidJSON test. Fixture: data/test.json, read through
// util::read_file so this also exercises the util::File + read_all path.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>
#include <util/io.hpp>

#include <string>

TEST_CASE("data/test.json parses and has the expected shape")
{
    const std::string text = util::read_file("data/test.json");
    REQUIRE(!text.empty());

    const nlohmann::json doc = nlohmann::json::parse(text);

    REQUIRE(doc.is_object());

    REQUIRE(doc.contains("id"));
    CHECK(doc["id"].is_string());
    CHECK(doc["id"].get<std::string>() == "1122334455");

    REQUIRE(doc.contains("value"));
    CHECK(doc["value"].is_number());
    CHECK(doc["value"].get<int>() == 123);
}

TEST_CASE("malformed JSON throws rather than returning a null document")
{
    CHECK_THROWS_AS(nlohmann::json::parse("{ not json"),
                    nlohmann::json::parse_error);
}

// This is the property that let us drop TOML: config files can carry comments.
// Without ignore_comments the same input is a parse error.
TEST_CASE("comments are accepted when ignore_comments is set")
{
    const char* commented = R"({
        "backend": "software",
        "fps": 60
    })";

    const char* with_comments = R"({
        // which renderer backend to prefer
        "backend": "software",
        /* frame cap; 0 means uncapped */
        "fps": 60
    })";

    CHECK_NOTHROW(nlohmann::json::parse(commented));
    CHECK_THROWS_AS(nlohmann::json::parse(with_comments),
                    nlohmann::json::parse_error);

    const nlohmann::json doc = nlohmann::json::parse(
        with_comments, nullptr, true, /*ignore_comments=*/true);

    CHECK(doc["backend"].get<std::string>() == "software");
    CHECK(doc["fps"].get<int>() == 60);
}
