// util::File and the read_all/read_file helpers.
//
// util::File selects its backend at compile time — util/posix/fileimpl.cc or
// util/mswin/fileimpl.cc — so this exercises whichever one was built.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/file.hpp>
#include <util/io.hpp>

#include <string>

TEST_CASE("opening an existing file succeeds")
{
    CHECK_NOTHROW(
        util::File(std::string("data/testfile"), util::OpenReadOnly));
}

TEST_CASE("opening a missing file throws")
{
    CHECK_THROWS(
        util::File(std::string("data/does-not-exist"), util::OpenReadOnly));
}

TEST_CASE("seek reports the file size")
{
    util::File file(std::string("data/testfile"), util::OpenReadOnly);

    const off_t size = file.seek(0, util::SeekEnd);
    CHECK(size == 15); // data/testfile is 15 bytes

    CHECK(file.seek(0, util::SeekSet) == 0);
}

TEST_CASE("read fills the buffer and advances the offset")
{
    util::File file(std::string("data/testfile"), util::OpenReadOnly);

    char buf[8] = {0};
    const ssize_t got = file.read(buf, sizeof(buf));

    REQUIRE(got == 8);
    CHECK(file.seek(0, util::SeekCur) == 8);
}

TEST_CASE("read_all recovers the whole file")
{
    util::File file(std::string("data/testfile"), util::OpenReadOnly);

    std::string contents;
    util::read_all(file, contents);

    CHECK(contents.size() == 15);
}

TEST_CASE("read_file matches read_all")
{
    const std::string via_helper = util::read_file("data/testfile");

    util::File file(std::string("data/testfile"), util::OpenReadOnly);
    std::string via_read_all;
    util::read_all(file, via_read_all);

    CHECK(via_helper == via_read_all);
}

// read_all's internal buffer is 8096 bytes, so a larger file exercises its
// multi-iteration path. data/teapot.obj is ~210 KB.
TEST_CASE("read_all handles files larger than its internal buffer")
{
    const std::string big = util::read_file("data/teapot.obj");

    CHECK(big.size() > 8096);
    CHECK(big.find("v ") != std::string::npos);
}
