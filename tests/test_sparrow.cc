// loaders::load_sparrow — the Sparrow / Starling atlas parser.
//
// The loader that has been commented out since 2016, waiting first for an XML
// dependency and then for gfx::renderer::Texture. It needs neither to be
// tested: it produces a frame table and a filename, so this runs with no video
// subsystem and under qemu, the same property that made tests/test_obj.cc
// possible.
//
// data/jetpackdude.xml is the fixture for the happy path, because the things
// worth pinning are properties of a real exporter's output rather than of a
// file written to match the parser. Trimming and the malformed cases are not in
// it — it is eight untrimmed frames — so those use scratch files rather than
// broken fixtures committed to data/.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <loaders/sparrow.hpp>
#include <posix/errors.hpp>

#include <cstdio>
#include <string>

namespace
{

// Deletes the scratch file however the scope exits, which for most cases here
// is by exception. Same reasoning as tests/test_obj.cc.
class ScratchFile
{
public:
    explicit ScratchFile(const std::string& text)
        : path_("test_sparrow.scratch.xml")
    {
        std::FILE* out = std::fopen(path_.c_str(), "w");
        REQUIRE(out != nullptr);
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }

    ~ScratchFile() { std::remove(path_.c_str()); }

    ScratchFile(const ScratchFile&) = delete;
    ScratchFile& operator=(const ScratchFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

// One SubTexture with whatever attributes the case is about, wrapped in the
// smallest valid document.
std::string atlas_of(const std::string& subtexture)
{
    return "<?xml version=\"1.0\"?>\n"
           "<TextureAtlas imagePath=\"sheet.png\">\n" +
           subtexture + "\n</TextureAtlas>\n";
}

} // namespace

//============================================================================
// The real fixture
//============================================================================

TEST_CASE("data/jetpackdude.xml parses as eight untrimmed frames")
{
    const loaders::SparrowAtlas atlas =
        loaders::load_sparrow("data/jetpackdude.xml");

    CHECK(atlas.frames.size() == 8);

    // Returned as authored. The sheet is not in the repository and this loader
    // does not care: resolving the path is the caller's job.
    CHECK(atlas.image_path == "JetPackDude.png");
}

TEST_CASE("data/jetpackdude.xml keeps document order and its frame geometry")
{
    const loaders::SparrowAtlas atlas =
        loaders::load_sparrow("data/jetpackdude.xml");
    REQUIRE(atlas.frames.size() == 8);

    for (std::size_t i = 0; i < atlas.frames.size(); ++i) {
        const gfx::AtlasFrame& frame = atlas.frames[i];

        // reddude.000 .. reddude.007, laid out in one row of 24x34 cells.
        CHECK(frame.id == "reddude.00" + std::to_string(i));
        CHECK(frame.source.x == static_cast<int>(i) * 24);
        CHECK(frame.source.y == 0);
        CHECK(frame.source.w == 24);
        CHECK(frame.source.h == 34);

        // No frame* attributes, so nothing is trimmed and the drawn size is the
        // region's own.
        CHECK_FALSE(frame.trimmed());
        CHECK(frame.trim_x == 0);
        CHECK(frame.trim_y == 0);
        CHECK(frame.width() == 24);
        CHECK(frame.height() == 34);
    }
}

TEST_CASE("data/foxy.xml parses as the generated Sunny Land atlas")
{
    // The counterpart fixture, and the one with an image behind it. Every frame
    // in it is trimmed, which is what jetpackdude.xml cannot cover.
    const loaders::SparrowAtlas atlas = loaders::load_sparrow("data/foxy.xml");

    CHECK(atlas.image_path == "foxy.png");
    CHECK(atlas.frames.size() == 36);

    // Frames of one animation are contiguous and in playback order, which is
    // what lets gfx::AnimatedSprite hold a first index and a count rather than
    // a list. tools/pack_atlas.py is what guarantees it.
    std::size_t run_start = 0;
    while (run_start < atlas.frames.size() &&
           atlas.frames[run_start].id != "run.000") {
        ++run_start;
    }
    REQUIRE(run_start < atlas.frames.size());

    for (std::size_t i = 0; i < 6; ++i) {
        const gfx::AtlasFrame& frame = atlas.frames[run_start + i];
        CHECK(frame.id == "run.00" + std::to_string(i));

        // Authored at 33x32 and trimmed, so the region is smaller than the
        // frame and the frame is the size layout must use.
        CHECK(frame.trimmed());
        CHECK(frame.frame_width == 33);
        CHECK(frame.frame_height == 32);
        CHECK(frame.source.w <= 33);
        CHECK(frame.source.h <= 32);

        // Positive after the loader negates Sparrow's sense, and the trimmed
        // region has to fit inside the frame it was cropped from.
        CHECK(frame.trim_x >= 0);
        CHECK(frame.trim_y >= 0);
        CHECK(frame.trim_x + frame.source.w <= frame.frame_width);
        CHECK(frame.trim_y + frame.source.h <= frame.frame_height);
    }
}

//============================================================================
// Trimming
//
// The part of the format most likely to be got wrong, because Sparrow states
// the offset in the opposite sense to the one a blit wants.
//============================================================================

TEST_CASE("frameX/frameY are negated into an offset to add")
{
    // A 24x34 sprite cropped to 20x30, with 2px removed from the left and 4
    // from the top. Sparrow writes that as frameX="-2" frameY="-4".
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"8\" y=\"16\" width=\"20\" "
                 "height=\"30\" frameX=\"-2\" frameY=\"-4\" frameWidth=\"24\" "
                 "frameHeight=\"34\"/>"));

    const loaders::SparrowAtlas atlas = loaders::load_sparrow(file.path());
    REQUIRE(atlas.frames.size() == 1);
    const gfx::AtlasFrame& frame = atlas.frames[0];

    CHECK(frame.trimmed());

    // Positive, so a caller adds it to the destination rather than having to
    // know which way round the format states it.
    CHECK(frame.trim_x == 2);
    CHECK(frame.trim_y == 4);

    // The region is still the region.
    CHECK(frame.source.x == 8);
    CHECK(frame.source.y == 16);
    CHECK(frame.source.w == 20);
    CHECK(frame.source.h == 30);

    // Layout uses the untrimmed size, which is the whole point of keeping it:
    // two frames trimmed differently must still occupy the same box.
    CHECK(frame.width() == 24);
    CHECK(frame.height() == 34);
}

TEST_CASE("a crop that took nothing off the top left still reads as trimmed")
{
    // frameX and frameY absent, frameWidth/frameHeight present: the crop only
    // removed transparent margin from the right and bottom. Legitimate, and the
    // frame size is still what layout wants.
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"20\" "
                 "height=\"30\" frameWidth=\"24\" frameHeight=\"34\"/>"));

    const loaders::SparrowAtlas atlas = loaders::load_sparrow(file.path());
    REQUIRE(atlas.frames.size() == 1);

    CHECK(atlas.frames[0].trimmed());
    CHECK(atlas.frames[0].trim_x == 0);
    CHECK(atlas.frames[0].trim_y == 0);
    CHECK(atlas.frames[0].width() == 24);
}

TEST_CASE("frameX without frameWidth is rejected rather than half-read")
{
    // An offset into a box whose size was never stated. Reading it as untrimmed
    // would drop the offset and misplace the sprite.
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"20\" "
                 "height=\"30\" frameX=\"-2\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("an untrimmed frame smaller than its trimmed region is rejected")
{
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"24\" "
                 "height=\"34\" frameWidth=\"20\" frameHeight=\"30\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

//============================================================================
// Rejected rather than mis-parsed
//============================================================================

TEST_CASE("a rotated frame is rejected, not drawn upright")
{
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"34\" "
                 "height=\"24\" rotated=\"true\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("rotated=\"false\" is the ordinary case and is not rejected")
{
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"24\" "
                 "height=\"34\" rotated=\"false\"/>"));

    CHECK_NOTHROW(loaders::load_sparrow(file.path()));
}

TEST_CASE("a frame with no area is rejected")
{
    const ScratchFile zero(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"0\" "
                 "height=\"34\"/>"));
    CHECK_THROWS_AS(loaders::load_sparrow(zero.path()),
                    loaders::SparrowFormatError);

    const ScratchFile negative(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"24\" "
                 "height=\"-34\"/>"));
    CHECK_THROWS_AS(loaders::load_sparrow(negative.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("a frame at a negative origin is rejected")
{
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"-1\" y=\"0\" width=\"24\" "
                 "height=\"34\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("a missing required attribute is rejected")
{
    // width absent. util::xml's require_attribute_int throws for this, and the
    // point of the case is that it arrives as SparrowFormatError rather than as
    // util::xml::ParseError — one type out of this function, not two.
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" "
                 "height=\"34\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("an unparseable attribute is rejected rather than read as zero")
{
    // This is what util::xml exists for: pugixml's own as_int() returns its
    // default for a malformed value, which would make this a 0x34 frame.
    const ScratchFile file(
        atlas_of("<SubTexture name=\"run.000\" x=\"0\" y=\"0\" width=\"24px\" "
                 "height=\"34\"/>"));

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("a document that is not a Sparrow atlas is rejected")
{
    // Well-formed XML, wrong root. data/test.xml is a real file of the wrong
    // shape, so this covers the case a caller actually hits: the right path
    // typed for the wrong asset.
    CHECK_THROWS_AS(loaders::load_sparrow("data/test.xml"),
                    loaders::SparrowFormatError);
}

TEST_CASE("a missing imagePath is rejected")
{
    const ScratchFile file("<TextureAtlas>\n"
                           "<SubTexture name=\"run.000\" x=\"0\" y=\"0\" "
                           "width=\"24\" height=\"34\"/>\n"
                           "</TextureAtlas>\n");

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("an atlas declaring no frames is rejected")
{
    const ScratchFile file(
        "<TextureAtlas imagePath=\"sheet.png\"></TextureAtlas>\n");

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("malformed XML is rejected")
{
    const ScratchFile file("<TextureAtlas imagePath=\"sheet.png\">");

    CHECK_THROWS_AS(loaders::load_sparrow(file.path()),
                    loaders::SparrowFormatError);
}

TEST_CASE("a file that cannot be read throws the posix error, not a format "
          "error")
{
    // An I/O failure is not a document failure, and util::xml::load draws that
    // line deliberately. Keeping it here means a missing asset does not present
    // as a corrupt one.
    CHECK_THROWS_AS(loaders::load_sparrow("data/does-not-exist.xml"),
                    posix::no_such_file);
}
