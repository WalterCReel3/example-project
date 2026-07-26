#include <util/xml.hpp>

#include <util/format.hpp>
#include <util/io.hpp>
#include <util/number.hpp>

#include <pugixml.hpp>

namespace util::xml
{

namespace
{

// The internal pointer a Node carries is exactly what pugi::xml_node wraps, so
// this is a reinterpretation of the same handle rather than a conversion.
pugi::xml_node wrap(pugi::xml_node_struct* node)
{
    return pugi::xml_node(node);
}

// Element name for a diagnostic. pugixml returns "" rather than null for an
// anonymous or null node, and "" reads badly mid-sentence.
const char* describe(const pugi::xml_node& node)
{
    const char* name = node.name();
    return (name && *name) ? name : "(anonymous element)";
}

} // namespace

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

const char* Node::name() const
{
    return wrap(node_).name();
}

const char* Node::text() const
{
    return wrap(node_).child_value();
}

Node Node::child(const char* name) const
{
    return Node(wrap(node_).child(name).internal_object());
}

Node Node::next_sibling(const char* name) const
{
    return Node(wrap(node_).next_sibling(name).internal_object());
}

Node Node::first_element() const
{
    for (pugi::xml_node child = wrap(node_).first_child(); child;
         child = child.next_sibling()) {
        if (child.type() == pugi::node_element) {
            return Node(child.internal_object());
        }
    }
    return Node();
}

Node Node::next_element() const
{
    for (pugi::xml_node sibling = wrap(node_).next_sibling(); sibling;
         sibling = sibling.next_sibling()) {
        if (sibling.type() == pugi::node_element) {
            return Node(sibling.internal_object());
        }
    }
    return Node();
}

bool Node::has_attribute(const char* name) const
{
    return !wrap(node_).attribute(name).empty();
}

const char* Node::attribute(const char* name, const char* fallback) const
{
    return wrap(node_).attribute(name).as_string(fallback);
}

// Deliberately not pugixml's as_int()/as_double(). Their documented contract is
// "the default value if conversion did not succeed or attribute is empty", but
// measured against v1.16 the default applies only when the attribute is
// *absent*: a present but non-numeric value yields 0, and "12px" yields 12.
// Routing through util::from_string makes malformed behave like absent, so the
// fallback means one thing.
int Node::attribute_int(const char* name, int fallback) const
{
    int value = fallback;
    from_string(attribute(name), value);
    return value;
}

double Node::attribute_double(const char* name, double fallback) const
{
    double value = fallback;
    from_string(attribute(name), value);
    return value;
}

bool Node::attribute_bool(const char* name, bool fallback) const
{
    return wrap(node_).attribute(name).as_bool(fallback);
}

const char* Node::require_attribute(const char* name) const
{
    const pugi::xml_node node = wrap(node_);
    const pugi::xml_attribute attribute = node.attribute(name);
    if (attribute.empty()) {
        throw ParseError(
            util::format("<%s> has no '%s' attribute", describe(node), name));
    }
    return attribute.value();
}

int Node::require_attribute_int(const char* name) const
{
    const char* text = require_attribute(name);
    int value = 0;
    if (!from_string(text, value)) {
        throw ParseError(
            util::format("<%s> attribute '%s' is not an integer: '%s'",
                         describe(wrap(node_)), name, text));
    }
    return value;
}

double Node::require_attribute_double(const char* name) const
{
    const char* text = require_attribute(name);
    double value = 0.0;
    if (!from_string(text, value)) {
        throw ParseError(
            util::format("<%s> attribute '%s' is not a number: '%s'",
                         describe(wrap(node_)), name, text));
    }
    return value;
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------

struct Document::Impl {
    pugi::xml_document document;
};

Document::Document()
    : impl_(new Impl())
{
}

// Out of line, because Impl is incomplete in the header. This is the whole
// reason the destructor and the move operations are not defaulted there.
Document::~Document() = default;
Document::Document(Document&& rh) noexcept = default;
Document& Document::operator=(Document&& rh) noexcept = default;

Node Document::document_element() const
{
    return Node(impl_->document.document_element().internal_object());
}

Node Document::child(const char* name) const
{
    return Node(impl_->document.child(name).internal_object());
}

NodeRange Document::children() const
{
    return NodeRange(Node(impl_->document.internal_object()).first_element(),
                     nullptr);
}

NodeRange Document::children(const char* name) const
{
    return NodeRange(child(name), name);
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

Document parse(const std::string& text)
{
    Document doc;
    const pugi::xml_parse_result result =
        doc.impl_->document.load_buffer(text.data(), text.size());
    if (!result) {
        throw ParseError(util::format("XML parse error at offset %ld: %s",
                                      static_cast<long>(result.offset),
                                      result.description()));
    }
    return doc;
}

Document load(const std::string& path)
{
    // Read through util::File rather than pugixml's load_file so that a missing
    // or unreadable asset produces the same typed posix:: exception as every
    // other file in the tree, and there is one I/O path to reason about.
    const std::string text = util::read_file(path.c_str());

    try {
        return parse(text);
    } catch (const ParseError& e) {
        // Rethrown rather than duplicating the parse call, so the path reaches
        // the message without a second copy of the error handling.
        throw ParseError(path + ": " + e.what());
    }
}

} // namespace util::xml
