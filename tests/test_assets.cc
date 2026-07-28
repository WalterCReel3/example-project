// rig::asset_path — where assets are read from.
//
// What is covered here is rule 1 (the WREEL_DATA_DIR override), the caching,
// and the separator handling, because those are reachable from a test process.
//
// What is NOT covered is rule 2, "data/ beside the executable". A test binary
// lives in build/<preset>/bin/ with no data/ next to it, and arranging one
// would mean either copying fixtures into the build tree or spawning a process
// per case. So rule 2 is exercised for the first time by an installed bundle,
// and the log line rig::asset_root() emits is how anyone finds out which rule
// won on a device. Stated rather than papered over — this file does not prove
// the shipped path works.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <rig/assets.hpp>

#include <cstdlib>
#include <string>

namespace
{

// cmake/Testing.cmake sets WREEL_DATA_DIR for every test, so each case here has
// to put it back or it changes the meaning of the next one.
class ScopedDataDir
{
public:
    explicit ScopedDataDir(const char* value)
        : saved_()
        , was_set_(false)
    {
        const char* previous = std::getenv("WREEL_DATA_DIR");
        if (previous) {
            saved_ = previous;
            was_set_ = true;
        }
        apply(value);
    }

    ~ScopedDataDir()
    {
        apply(was_set_ ? saved_.c_str() : nullptr);
        rig::reset_asset_root();
    }

    ScopedDataDir(const ScopedDataDir&) = delete;
    ScopedDataDir& operator=(const ScopedDataDir&) = delete;

private:
    static void apply(const char* value)
    {
        if (value) {
            ::setenv("WREEL_DATA_DIR", value, 1);
        } else {
            ::unsetenv("WREEL_DATA_DIR");
        }
        rig::reset_asset_root();
    }

    std::string saved_;
    bool was_set_;
};

} // namespace

TEST_CASE("WREEL_DATA_DIR wins, and gains a trailing separator")
{
    const ScopedDataDir guard("/tmp/wreel-assets");

    CHECK(rig::asset_root() == "/tmp/wreel-assets/");
}

TEST_CASE("an existing trailing separator is not doubled")
{
    const ScopedDataDir guard("/tmp/wreel-assets/");

    CHECK(rig::asset_root() == "/tmp/wreel-assets/");
}

TEST_CASE("asset_path joins the root and the name")
{
    const ScopedDataDir guard("/tmp/wreel-assets");

    CHECK(rig::asset_path("Speedy.fon") == "/tmp/wreel-assets/Speedy.fon");

    // Names are relative to the root and do not carry "data/" themselves; the
    // directory is part of the layout being resolved, not part of the name.
    CHECK(rig::asset_path("her bloody weekend.mod") ==
          "/tmp/wreel-assets/her bloody weekend.mod");
}

TEST_CASE("an empty WREEL_DATA_DIR is ignored rather than yielding \"/\"")
{
    const ScopedDataDir guard("");

    // Set-but-empty is how an unset variable often arrives out of a shell
    // script, and treating it as a root would resolve every asset against the
    // filesystem root.
    CHECK(rig::asset_root() != "/");
}

TEST_CASE("the root is resolved once and cached")
{
    const ScopedDataDir guard("/tmp/wreel-first");
    REQUIRE(rig::asset_root() == "/tmp/wreel-first/");

    // Changing the environment behind its back must not change the answer: the
    // root is fixed for the run, so a mid-run change cannot leave some assets
    // loaded from one tree and some from another.
    ::setenv("WREEL_DATA_DIR", "/tmp/wreel-second", 1);
    CHECK(rig::asset_root() == "/tmp/wreel-first/");

    // And the test hook does re-resolve, which is the only reason it exists.
    rig::reset_asset_root();
    CHECK(rig::asset_root() == "/tmp/wreel-second/");
}

TEST_CASE("the root always ends in a separator, whichever rule won")
{
    // No override, so this falls to rule 2 or rule 3 depending on where the
    // binary sits. Both must produce a joinable root — asset_path() does a
    // plain concatenation and relies on it.
    const ScopedDataDir guard(nullptr);

    const std::string& root = rig::asset_root();
    REQUIRE_FALSE(root.empty());
    CHECK(root.back() == '/');
    CHECK(rig::asset_path("x") == root + "x");
}
