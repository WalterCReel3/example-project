// loaders::load_obj — the Wavefront OBJ parser.
//
// The loader's first tests. It could not have them before: it filled a
// gfx::ObjModel holding GLuint buffer handles, so it only built when a GL
// backend was configured and only ran where there was a GPU. Producing
// gfx::Mesh made it plain data, and this file is the point of that change.
//
// The counts for ico.obj and teapot.obj are the ones recorded in
// docs/DEVELOPMENT.md from before the C++17 tokenizer work, so they pin the
// geometry across that change too.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <loaders/obj.hpp>
#include <posix/errors.hpp>

#include <cstdio>
#include <string>

namespace
{

// Deletes the scratch file however the scope exits. Most cases here expect
// load_obj to throw, so a plain remove() after the call is skipped exactly when
// it is needed — which is how the first version of this file left
// test_obj.scratch.obj in the working tree.
class ScratchFile
{
public:
    explicit ScratchFile(const std::string& text)
        : path_("test_obj.scratch.obj")
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

// Writes an OBJ to a scratch file and loads it, so malformed input can be
// tested without committing broken fixtures to data/.
gfx::Mesh load_text(const std::string& text)
{
    const ScratchFile scratch(text);
    return loaders::load_obj(scratch.path());
}

} // namespace

TEST_CASE("data/cube.obj")
{
    const gfx::Mesh mesh = loaders::load_obj("data/cube.obj");

    CHECK(mesh.vertices.size() == 8);
    // 12 triangles for six quad faces, which is what the fixture's `f` lines
    // already spell out as triangles.
    CHECK(mesh.indexes.size() == 36);
    CHECK(mesh.colors.size() == mesh.vertices.size());
    CHECK(mesh.triangulated());
    CHECK(mesh.indexes_in_range());
}

// The values recorded in docs/DEVELOPMENT.md § Running the skratch demo.
TEST_CASE("data/ico.obj matches the counts recorded before the C++17 work")
{
    const gfx::Mesh mesh = loaders::load_obj("data/ico.obj");

    CHECK(mesh.vertices.size() == 42);
    CHECK(mesh.indexes.size() == 240);
    CHECK(mesh.colors.size() == 42);
    CHECK(mesh.triangulated());
    CHECK(mesh.indexes_in_range());
}

TEST_CASE("data/teapot.obj matches the counts recorded before the C++17 work")
{
    const gfx::Mesh mesh = loaders::load_obj("data/teapot.obj");

    CHECK(mesh.vertices.size() == 3644);
    CHECK(mesh.indexes.size() == 18960);
    CHECK(mesh.colors.size() == 3644);
    CHECK(mesh.triangulated());
    CHECK(mesh.indexes_in_range());
}

TEST_CASE("vertex coordinates are read in order, including negatives")
{
    const gfx::Mesh mesh = load_text("v 1.0 2.0 3.0\n"
                                     "v -1.5 0 2.25\n"
                                     "f 1 2 1\n");

    REQUIRE(mesh.vertices.size() == 2);
    CHECK(mesh.vertices[0].x == doctest::Approx(1.0f));
    CHECK(mesh.vertices[0].y == doctest::Approx(2.0f));
    CHECK(mesh.vertices[0].z == doctest::Approx(3.0f));
    CHECK(mesh.vertices[1].x == doctest::Approx(-1.5f));
    CHECK(mesh.vertices[1].y == doctest::Approx(0.0f));
    CHECK(mesh.vertices[1].z == doctest::Approx(2.25f));
}

TEST_CASE("face indexes are converted from 1-based to 0-based")
{
    const gfx::Mesh mesh = load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                     "f 1 2 3\n");

    REQUIRE(mesh.indexes.size() == 3);
    CHECK(mesh.indexes[0] == 0);
    CHECK(mesh.indexes[1] == 1);
    CHECK(mesh.indexes[2] == 2);
}

// The 2016 parser handled this by accident: strtol stops at '/', so the form
// parsed and nothing recorded that it was supported. Strict number parsing
// would have rejected it, which would fail on files that work in every other
// tool.
TEST_CASE("the v/vt/vn face form reads the vertex index and ignores the rest")
{
    const gfx::Mesh mesh = load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                     "vt 0 0\nvn 0 0 1\n"
                                     "f 1/1/1 2/1/1 3/1/1\n");

    REQUIRE(mesh.vertices.size() == 3);
    REQUIRE(mesh.indexes.size() == 3);
    CHECK(mesh.indexes[0] == 0);
    CHECK(mesh.indexes[1] == 1);
    CHECK(mesh.indexes[2] == 2);

    // Missing texture/normal fields are fine too: "f 1// 2// 3//".
    const gfx::Mesh sparse = load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                       "f 1// 2// 3//\n");
    CHECK(sparse.indexes.size() == 3);
}

// The original took the first three vertices of a polygon and dropped the rest,
// which leaves a hole in the model rather than an error.
TEST_CASE("a quad face is triangulated as a fan rather than truncated")
{
    const gfx::Mesh mesh = load_text("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                                     "f 1 2 3 4\n");

    REQUIRE(mesh.indexes.size() == 6);
    CHECK(mesh.indexes[0] == 0);
    CHECK(mesh.indexes[1] == 1);
    CHECK(mesh.indexes[2] == 2);
    CHECK(mesh.indexes[3] == 0);
    CHECK(mesh.indexes[4] == 2);
    CHECK(mesh.indexes[5] == 3);
    CHECK(mesh.triangulated());
}

TEST_CASE("unknown keywords, comments and blank lines are ignored")
{
    const gfx::Mesh mesh = load_text("# a comment\n"
                                     "mtllib something.mtl\n"
                                     "o object_name\n"
                                     "\n"
                                     "v 0 0 0\n"
                                     "vn 0 0 1\n"
                                     "vt 0.5 0.5\n"
                                     "s off\n"
                                     "usemtl material\n"
                                     "g group\n"
                                     "v 1 0 0\n"
                                     "v 0 1 0\n"
                                     "f 1 2 3\n");

    CHECK(mesh.vertices.size() == 3);
    CHECK(mesh.indexes.size() == 3);
}

// A blank line between the vertex and face blocks was claimed to truncate the
// model through util::line_iterator (D4). It does not, and this is the
// end-to-end half of that verification.
TEST_CASE("a blank line between blocks does not truncate the model")
{
    const gfx::Mesh mesh = load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                     "\n"
                                     "f 1 2 3\n");

    CHECK(mesh.vertices.size() == 3);
    CHECK(mesh.indexes.size() == 3);
}

TEST_CASE("an empty file is an empty mesh, not an error")
{
    const gfx::Mesh mesh = load_text("");

    CHECK(mesh.vertices.empty());
    CHECK(mesh.indexes.empty());
    CHECK(mesh.triangulated());
    CHECK(mesh.indexes_in_range());
}

// Everything below used to be accepted and quietly produce wrong geometry: the
// 2016 parser passed every field through strtod/strtol without checking, so a
// malformed coordinate became 0.0 and a malformed index became 0.
TEST_CASE("a malformed coordinate is an error rather than 0.0")
{
    CHECK_THROWS_AS(load_text("v 1.0 nonsense 3.0\n"), loaders::ObjFormatError);
    CHECK_THROWS_AS(load_text("v 1.0 2.0 1.0.0\n"), loaders::ObjFormatError);
    CHECK_THROWS_AS(load_text("v 1.0 2.0 3.0px\n"), loaders::ObjFormatError);
}

TEST_CASE("a vertex with too few coordinates is an error")
{
    CHECK_THROWS_AS(load_text("v 1.0 2.0\n"), loaders::ObjFormatError);
    CHECK_THROWS_AS(load_text("v\n"), loaders::ObjFormatError);
}

TEST_CASE("a malformed or out-of-range face index is an error")
{
    CHECK_THROWS_AS(load_text("v 0 0 0\nf 1 x 1\n"), loaders::ObjFormatError);
    CHECK_THROWS_AS(load_text("v 0 0 0\nf 1 2\n"), loaders::ObjFormatError);

    // OBJ counts from 1, so 0 is never a vertex.
    CHECK_THROWS_AS(load_text("v 0 0 0\nf 0 1 1\n"), loaders::ObjFormatError);

    // End-relative indexes are legal OBJ and not implemented here; saying so
    // beats resolving -1 to 4294967294.
    CHECK_THROWS_AS(load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -1 -2 -3\n"),
                    loaders::ObjFormatError);

    // Addressing a vertex the file never declares.
    CHECK_THROWS_AS(load_text("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n"),
                    loaders::ObjFormatError);
}

TEST_CASE("the error message names the file and the line")
{
    // A diagnostic without a line number is nearly useless on a 10,000-line
    // mesh.
    try {
        load_text("v 0 0 0\nv 0 0 0\nv 1 nonsense 1\n");
        FAIL("expected ObjFormatError");
    } catch (const loaders::ObjFormatError& e) {
        const std::string what = e.what();
        CHECK(what.find("test_obj.scratch.obj") != std::string::npos);
        CHECK(what.find(":3:") != std::string::npos);
        CHECK(what.find("nonsense") != std::string::npos);
    }
}

// An I/O failure keeps its own type rather than arriving as a format error, the
// same distinction util::xml::load draws.
TEST_CASE("a missing file throws the posix error, not ObjFormatError")
{
    CHECK_THROWS_AS(loaders::load_obj("data/does-not-exist.obj"),
                    posix::no_such_file);
}
