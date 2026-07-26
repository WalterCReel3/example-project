// util::xml — the pugixml facade.
//
// Fixtures are the XML already in data/: jetpackdude.xml is a real Sparrow
// atlas as exported by ShoeBox, which is the format loaders::load_sparrow will
// read, so this pins the shape that loader depends on before it is written.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <posix/errors.hpp>
#include <util/xml.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

TEST_CASE("data/jetpackdude.xml parses as the Sparrow atlas it is")
{
    const util::xml::Document doc = util::xml::load("data/jetpackdude.xml");

    const util::xml::Node atlas = doc.child("TextureAtlas");
    REQUIRE(atlas);
    CHECK(std::string(atlas.name()) == "TextureAtlas");

    // The sheet this atlas names is not in the repository -- the atlas is
    // orphaned. That is a fixture problem, not a parsing one, so it is asserted
    // here rather than quietly worked around.
    CHECK(std::string(atlas.require_attribute("imagePath")) ==
          "JetPackDude.png");

    int frames = 0;
    for (const util::xml::Node& frame : atlas.children("SubTexture")) {
        CHECK(frame.require_attribute_int("width") == 24);
        CHECK(frame.require_attribute_int("height") == 34);
        CHECK(frame.require_attribute_int("y") == 0);
        // Frames are laid out left to right, 24px apart.
        CHECK(frame.require_attribute_int("x") == frames * 24);
        ++frames;
    }
    CHECK(frames == 8);
}

TEST_CASE("children() composes with the standard algorithms")
{
    const util::xml::Document doc = util::xml::load("data/jetpackdude.xml");
    const util::xml::NodeRange frames =
        doc.child("TextureAtlas").children("SubTexture");

    // The property pugixml was chosen for: a named-child range is a real
    // iterator pair, not a callback or an out-parameter.
    CHECK(std::distance(frames.begin(), frames.end()) == 8);

    std::vector<std::string> names;
    std::transform(frames.begin(), frames.end(), std::back_inserter(names),
                   [](const util::xml::Node& n) {
                       return std::string(n.require_attribute("name"));
                   });

    REQUIRE(names.size() == 8);
    CHECK(names.front() == "reddude.000");
    CHECK(names.back() == "reddude.007");

    const auto found = std::find_if(
        frames.begin(), frames.end(), [](const util::xml::Node& n) {
            return std::string(n.attribute("name")) == "reddude.005";
        });
    REQUIRE(found != frames.end());
    CHECK(found->require_attribute_int("x") == 120);
}

TEST_CASE("data/test2.xml — attributes and children on a hand-written file")
{
    const util::xml::Document doc = util::xml::load("data/test2.xml");

    const util::xml::Node root = doc.document_element();
    REQUIRE(root);
    CHECK(std::string(root.name()) == "TextureAtlas");
    CHECK(std::string(root.attribute("path")) == "images/something.png");

    int seen = 0;
    for (const util::xml::Node& sub : root.children("SubTexture")) {
        ++seen;
        CHECK(std::string(sub.require_attribute("name")) ==
              "What" + std::to_string(seen));
    }
    CHECK(seen == 5);
}

TEST_CASE("data/test.xml — a document whose root has no children")
{
    const util::xml::Document doc = util::xml::load("data/test.xml");

    const util::xml::Node root = doc.document_element();
    REQUIRE(root);
    CHECK(std::string(root.name()) == "document");
    CHECK(std::string(root.attribute("name")) == "test");

    // The root contains only whitespace, which pugixml's default options do not
    // add to the tree, so there is no element child to find.
    CHECK(root.children("anything").begin() == root.children("anything").end());
    CHECK(root.first_element().empty());
}

TEST_CASE("document_element is the outermost element, not the declaration")
{
    // jetpackdude.xml opens with <?xml version="1.0" ?>. parse_declaration is
    // off by default, so the declaration is not in the tree at all -- but
    // document_element() would be the right answer even if it were.
    const util::xml::Document doc = util::xml::load("data/jetpackdude.xml");
    CHECK(std::string(doc.document_element().name()) == "TextureAtlas");
}

TEST_CASE("a null node is safe to walk and yields nothing")
{
    const util::xml::Node null;

    CHECK(null.empty());
    CHECK_FALSE(static_cast<bool>(null));
    CHECK(std::string(null.name()).empty());
    CHECK(std::string(null.text()).empty());
    CHECK(null.child("anything").empty());
    CHECK(null.next_sibling("anything").empty());
    CHECK(null.first_element().empty());
    CHECK(null.next_element().empty());
    CHECK_FALSE(null.has_attribute("anything"));
    CHECK(std::string(null.attribute("anything", "fallback")) == "fallback");
    CHECK(null.attribute_int("anything", -1) == -1);

    // Iterating a null node's children terminates immediately rather than
    // dereferencing anything.
    int visited = 0;
    for (const util::xml::Node& child : null.children()) {
        (void)child;
        ++visited;
    }
    CHECK(visited == 0);
}

TEST_CASE("children() skips character data, so callers need no type test")
{
    const util::xml::Document doc =
        util::xml::parse("<map>text before<layer id=\"1\"/>text between"
                         "<![CDATA[cdata]]><layer id=\"2\"/></map>");

    const util::xml::Node map = doc.child("map");
    REQUIRE(map);

    std::vector<int> ids;
    for (const util::xml::Node& child : map.children()) {
        ids.push_back(child.require_attribute_int("id"));
    }
    CHECK(ids == std::vector<int>{1, 2});
}

TEST_CASE("text() returns the element's character data")
{
    // How a TMX CSV layer arrives. Whitespace is deliberately not trimmed.
    const util::xml::Document doc =
        util::xml::parse("<data encoding=\"csv\">\n1,2,3\n</data>");

    const util::xml::Node data = doc.child("data");
    REQUIRE(data);
    CHECK(std::string(data.attribute("encoding")) == "csv");
    CHECK(std::string(data.text()) == "\n1,2,3\n");
}

TEST_CASE("an absent attribute yields the fallback, not a throw")
{
    const util::xml::Document doc = util::xml::parse("<e present=\"7\"/>");
    const util::xml::Node e = doc.child("e");

    CHECK(e.has_attribute("present"));
    CHECK_FALSE(e.has_attribute("absent"));

    CHECK(std::string(e.attribute("absent")).empty());
    CHECK(std::string(e.attribute("absent", "fallback")) == "fallback");
    CHECK(e.attribute_int("absent", -1) == -1);
    CHECK(e.attribute_double("absent", 1.5) == doctest::Approx(1.5));
    CHECK(e.attribute_bool("absent", true));

    CHECK(e.attribute_int("present") == 7);
}

// This is the case the require_* accessors exist for. pugixml's as_int()
// returns its default for an absent attribute, for a malformed one, and for a
// legitimate zero -- so an asset with a missing width reads as 0 and produces
// an invisible sprite instead of an error.
TEST_CASE("require_attribute distinguishes absent from malformed from zero")
{
    const util::xml::Document doc =
        util::xml::parse("<SubTexture width=\"nonsense\" height=\"0\"/>");
    const util::xml::Node e = doc.child("SubTexture");

    CHECK_THROWS_AS(e.require_attribute("absent"), util::xml::ParseError);
    CHECK_THROWS_AS(e.require_attribute_int("absent"), util::xml::ParseError);

    // Present but not a number.
    CHECK_THROWS_AS(e.require_attribute_int("width"), util::xml::ParseError);
    CHECK_THROWS_AS(e.require_attribute_double("width"), util::xml::ParseError);
    // Whereas the defaulted accessor cannot tell you that.
    CHECK(e.attribute_int("width", -1) == -1);

    // A real zero is a value, not a failure.
    CHECK(e.require_attribute_int("height") == 0);

    // Trailing garbage is malformed rather than a partial read, which is the
    // difference between from_chars-with-end-check and atoi.
    const util::xml::Document trailing = util::xml::parse("<e x=\"12px\"/>");
    CHECK_THROWS_AS(trailing.child("e").require_attribute_int("x"),
                    util::xml::ParseError);
}

TEST_CASE("require_attribute_int handles negatives and the int edges")
{
    const util::xml::Document doc =
        util::xml::parse("<e neg=\"-42\" min=\"-2147483648\" "
                         "max=\"2147483647\" over=\"2147483648\"/>");
    const util::xml::Node e = doc.child("e");

    CHECK(e.require_attribute_int("neg") == -42);
    CHECK(e.require_attribute_int("min") == -2147483648);
    CHECK(e.require_attribute_int("max") == 2147483647);
    // Out of range is a failure, not a wrap.
    CHECK_THROWS_AS(e.require_attribute_int("over"), util::xml::ParseError);
}

TEST_CASE(
    "require_attribute_double parses what TMX object coordinates look like")
{
    const util::xml::Document doc =
        util::xml::parse("<object x=\"32.5\" y=\"-0.25\" rotation=\"0\"/>");
    const util::xml::Node o = doc.child("object");

    CHECK(o.require_attribute_double("x") == doctest::Approx(32.5));
    CHECK(o.require_attribute_double("y") == doctest::Approx(-0.25));
    CHECK(o.require_attribute_double("rotation") == doctest::Approx(0.0));
}

TEST_CASE("the error message names the element and the attribute")
{
    const util::xml::Document doc = util::xml::parse("<SubTexture x=\"n/a\"/>");
    const util::xml::Node e = doc.child("SubTexture");

    // A diagnostic that says only "parse error" costs an hour on a broken
    // asset.
    try {
        e.require_attribute_int("x");
        FAIL("expected ParseError");
    } catch (const util::xml::ParseError& err) {
        const std::string what = err.what();
        CHECK(what.find("SubTexture") != std::string::npos);
        CHECK(what.find("'x'") != std::string::npos);
        CHECK(what.find("n/a") != std::string::npos);
    }
}

TEST_CASE("a malformed document throws ParseError with the offset")
{
    CHECK_THROWS_AS(util::xml::parse("<unclosed>"), util::xml::ParseError);
    CHECK_THROWS_AS(util::xml::parse("<a></b>"), util::xml::ParseError);
    CHECK_THROWS_AS(util::xml::parse("not xml at all"), util::xml::ParseError);
    CHECK_THROWS_AS(util::xml::parse(""), util::xml::ParseError);

    try {
        util::xml::parse("<a><b></a>");
        FAIL("expected ParseError");
    } catch (const util::xml::ParseError& err) {
        const std::string what = err.what();
        CHECK(what.find("offset") != std::string::npos);
    }
}

TEST_CASE("load names the path in a parse failure")
{
    // data/test.json is valid JSON and therefore not valid XML, which makes it
    // a fixture for the failure path without adding a broken file to data/.
    try {
        util::xml::load("data/test.json");
        FAIL("expected ParseError");
    } catch (const util::xml::ParseError& err) {
        CHECK(std::string(err.what()).find("data/test.json") !=
              std::string::npos);
    }
}

// An I/O failure is not a document failure, so it keeps its own type rather
// than arriving as a ParseError. Same distinction util::File already draws.
TEST_CASE("a missing file throws the posix error, not ParseError")
{
    CHECK_THROWS_AS(util::xml::load("data/does-not-exist.xml"),
                    posix::no_such_file);
}

TEST_CASE("a Document is movable, and moving carries the tree")
{
    util::xml::Document doc = util::xml::parse("<root a=\"1\"/>");
    CHECK(doc.child("root").require_attribute_int("a") == 1);

    const util::xml::Document moved = std::move(doc);
    CHECK(moved.child("root").require_attribute_int("a") == 1);
}

TEST_CASE("iterator semantics: post-increment, copies and multipass")
{
    const util::xml::Document doc =
        util::xml::parse("<r><c n=\"1\"/><c n=\"2\"/><c n=\"3\"/></r>");
    const util::xml::NodeRange cs = doc.child("r").children("c");

    util::xml::NodeIterator i = cs.begin();
    const util::xml::NodeIterator before = i++;

    // Post-increment returns the previous position and it must outlive the
    // expression -- the same defect that was D2 in util::line_iterator.
    CHECK(before->require_attribute_int("n") == 1);
    CHECK(i->require_attribute_int("n") == 2);

    // Forward iterator, so a copy is an independent cursor and the range can be
    // traversed more than once.
    util::xml::NodeIterator copy = i;
    ++copy;
    CHECK(copy->require_attribute_int("n") == 3);
    CHECK(i->require_attribute_int("n") == 2);

    CHECK(std::distance(cs.begin(), cs.end()) == 3);
    CHECK(std::distance(cs.begin(), cs.end()) == 3);
}

TEST_CASE("nested elements of the same name do not bleed into one range")
{
    // A TMX shape: <map><layer/><group><layer/></group><layer/></map>. The
    // range over map's layers must not descend into the group.
    const util::xml::Document doc = util::xml::parse(
        "<map><layer id=\"1\"/><group><layer id=\"99\"/></group>"
        "<layer id=\"2\"/></map>");

    std::vector<int> ids;
    for (const util::xml::Node& layer : doc.child("map").children("layer")) {
        ids.push_back(layer.require_attribute_int("id"));
    }
    CHECK(ids == std::vector<int>{1, 2});

    CHECK(doc.child("map").child("group").child("layer").require_attribute_int(
              "id") == 99);
}
