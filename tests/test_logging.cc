// util::logging — level filtering and the file sink.
//
// The interesting properties are the ones that were wrong or absent before:
// that a suppressed message produces no output at all, that the sink can be
// redirected to a file and back, and that a failed open leaves logging usable
// rather than silently dead.
//
// Reading the log back requires the file sink, so these tests write to a
// temporary path rather than trying to capture stderr.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/logging.hpp>
#include <util/io.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{

const char* const log_path = "test_logging.out.txt";

// Closes the sink first: the contents are only guaranteed complete once the
// FILE* is closed, since only errors are flushed eagerly.
std::string read_log()
{
    util::log_close_file();
    return util::read_file(log_path);
}

struct LogFixture {
    LogFixture()
        : saved_level(util::log_level())
    {
        REQUIRE(util::log_open_file(log_path));
    }

    ~LogFixture()
    {
        util::log_close_file();
        util::log_set_level(saved_level);
        std::remove(log_path);
    }

    util::LogLevel saved_level;
};

} // namespace

TEST_CASE("the default level suppresses everything below error")
{
    // Matches the previous logger's default, which mattered enough to preserve.
    LogFixture fixture;
    CHECK(util::log_level() == util::LogError);

    CHECK(util::log_enabled(util::LogError));
    CHECK_FALSE(util::log_enabled(util::LogWarning));
    CHECK_FALSE(util::log_enabled(util::LogInfo));
    CHECK_FALSE(util::log_enabled(util::LogDebug));
}

TEST_CASE("a suppressed message produces no output")
{
    LogFixture fixture;
    util::log_set_level(util::LogError);

    util::log_debug("debug %d", 1);
    util::log_info("info %d", 2);
    util::log_warning("warning %d", 3);

    CHECK(read_log().empty());
}

TEST_CASE("messages at or below the level are emitted")
{
    LogFixture fixture;
    util::log_set_level(util::LogInfo);

    util::log_error("an error");
    util::log_warning("a warning");
    util::log_info("some info");
    util::log_debug("a debug line"); // above the level, dropped

    const std::string contents = read_log();

    CHECK(contents.find("an error") != std::string::npos);
    CHECK(contents.find("a warning") != std::string::npos);
    CHECK(contents.find("some info") != std::string::npos);
    CHECK(contents.find("a debug line") == std::string::npos);
}

TEST_CASE("each level is tagged distinctly")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    util::log_error("e");
    util::log_warning("w");
    util::log_info("i");
    util::log_debug("d");

    const std::string contents = read_log();

    CHECK(contents.find("[E] e") != std::string::npos);
    CHECK(contents.find("[W] w") != std::string::npos);
    CHECK(contents.find("[I] i") != std::string::npos);
    CHECK(contents.find("[D] d") != std::string::npos);
}

TEST_CASE("format conversions reach the output")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    util::log_info("%s %d %.2f %08x", "text", 42, 1.5, 0xABCDu);

    const std::string contents = read_log();
    CHECK(contents.find("text 42 1.50 0000abcd") != std::string::npos);
}

TEST_CASE("each message occupies its own line")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    util::log_info("first");
    util::log_info("second");
    util::log_info("third");

    const std::string contents = read_log();
    CHECK(std::count(contents.begin(), contents.end(), '\n') == 3);
}

TEST_CASE("an over-long message is truncated and marked, not split")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    const std::string huge(4000, 'x');
    util::log_info("%s", huge.c_str());

    const std::string contents = read_log();

    // One line, capped at the 1024-byte message limit including its newline.
    CHECK(std::count(contents.begin(), contents.end(), '\n') == 1);
    CHECK(contents.size() <= 1024);
    // The cut is visible, so a truncated line cannot pass as a complete one.
    CHECK(contents.find("...\n") != std::string::npos);
}

TEST_CASE("a message exactly at the limit still terminates")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    // "[I] " is 4 bytes, so this lands the body right at the boundary.
    const std::string body(1024 - 4 - 1, 'y');
    util::log_info("%s", body.c_str());

    const std::string contents = read_log();
    REQUIRE_FALSE(contents.empty());
    CHECK(contents.back() == '\n');
    CHECK(contents.size() <= 1024);
}

TEST_CASE("a failed open leaves the previous sink in place")
{
    const util::LogLevel saved = util::log_level();

    // A directory that does not exist cannot be opened for writing.
    CHECK_FALSE(util::log_open_file("no/such/directory/log.txt"));

    // Still usable afterwards — the failure must not leave a dangling sink.
    util::log_set_level(util::LogDebug);
    CHECK_NOTHROW(util::log_info("still alive"));

    util::log_set_level(saved);
}

TEST_CASE("closing an unopened sink is harmless")
{
    CHECK_NOTHROW(util::log_close_file());
    CHECK_NOTHROW(util::log_close_file());
}

TEST_CASE("reopening truncates rather than appending")
{
    LogFixture fixture;
    util::log_set_level(util::LogDebug);

    util::log_info("first run");
    util::log_close_file();
    REQUIRE(util::log_open_file(log_path));
    util::log_info("second run");

    const std::string contents = read_log();
    CHECK(contents.find("second run") != std::string::npos);
    CHECK(contents.find("first run") == std::string::npos);
}
