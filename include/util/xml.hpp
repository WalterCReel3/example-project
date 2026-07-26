#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

// pugixml's own forward declaration, verified against v1.16 pugixml.hpp:312.
// Holding the internal node pointer rather than a pugi::xml_node is what keeps
// pugixml.hpp out of every consumer's translation unit; it is the one place
// this facade names the vendor namespace, and it appears in no signature.
namespace pugi
{
struct xml_node_struct;
}

//============================================================================
//
// XML
//
// A facade over pugixml, for the two asset formats in the 2D pipeline that are
// XML and are not ours to redefine: Sparrow texture atlases and Tiled TMX maps.
// See docs/TARGETS.md § "XML: why pugixml".
//
//     const util::xml::Document doc = util::xml::load("data/jetpackdude.xml");
//     for (const util::xml::Node& frame :
//              doc.child("TextureAtlas").children("SubTexture")) {
//         const int w = frame.require_attribute_int("width");
//     }
//
// Two lifetime rules, neither of which the compiler will enforce:
//
//   - A Node is a non-owning view into its Document. Every Node obtained from a
//     Document dangles once that Document is destroyed or moved from. This
//     mirrors pugixml, where the document owns the tree and nodes are handles.
//   - Element and attribute names are taken as const char* and stored by
//     pointer, so a name passed to children() must outlive the range. String
//     literals, which is what call sites actually use, always do.
//
// Strings returned by name(), text() and the attribute accessors point into
// document-owned memory and are never null — an absent value reads as "",
// matching pugixml's contract. They are valid for the lifetime of the Document.
//
//============================================================================
namespace util::xml
{

// Thrown for a malformed document, and by the require_* accessors for an absent
// or unparseable attribute. Consistent with the rest of the tree, where errors
// are types rather than return codes — see include/posix/errors.hpp.
class ParseError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class NodeRange;

class Node
{
public:
    // A default-constructed Node is the null node: empty(), no name, no
    // children. It is what a failed lookup returns, and what end() compares to.
    Node() = default;

    bool empty() const { return node_ == nullptr; }
    explicit operator bool() const { return node_ != nullptr; }

    // Compares the underlying node identity, not the element contents.
    bool operator==(const Node& rh) const { return node_ == rh.node_; }
    bool operator!=(const Node& rh) const { return node_ != rh.node_; }

    const char* name() const;

    // The element's character data: "1,2,3" for <data>1,2,3</data>, which is
    // how a TMX CSV layer arrives. Leading and trailing whitespace is *not*
    // trimmed, because the document is parsed with pugixml's default options.
    const char* text() const;

    // The first child element with this name, or the null node.
    Node child(const char* name) const;

    // The next sibling element with this name, or the null node. Mainly the
    // step function behind NodeIterator.
    Node next_sibling(const char* name) const;

    // Element children only: character data, CDATA and comments are skipped, so
    // a caller iterating children never has to test the node type.
    Node first_element() const;
    Node next_element() const;

    bool has_attribute(const char* name) const;

    // Absent attributes yield the fallback. So does one that is present but
    // unparseable as the requested type -- the two cases are deliberately not
    // distinguished here, which is why the require_* forms below exist. For an
    // asset, a missing width silently reading as 0 produces an invisible sprite
    // rather than a diagnosable failure.
    const char* attribute(const char* name, const char* fallback = "") const;
    int attribute_int(const char* name, int fallback = 0) const;
    double attribute_double(const char* name, double fallback = 0.0) const;
    bool attribute_bool(const char* name, bool fallback = false) const;

    // Throw ParseError naming the element and the attribute rather than
    // returning a fallback. These distinguish absent from malformed, which
    // pugixml's as_int() cannot: it returns its default for both.
    const char* require_attribute(const char* name) const;
    int require_attribute_int(const char* name) const;
    double require_attribute_double(const char* name) const;

    // Defined below, once NodeRange is complete.
    NodeRange children() const;
    NodeRange children(const char* name) const;

private:
    friend class Document;

    explicit Node(pugi::xml_node_struct* node)
        : node_(node)
    {
    }

    pugi::xml_node_struct* node_ = nullptr;
};

// Forward iterator over sibling elements. A null name_ means "any element",
// which is what Node::children() with no argument produces.
//
// Holding the current Node by value is what lets reference be a real Node& and
// therefore makes this a forward rather than an input iterator. It is the same
// shape pugixml's own xml_named_node_iterator uses.
class NodeIterator
{
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef Node value_type;
    typedef std::ptrdiff_t difference_type;
    typedef const Node* pointer;
    typedef const Node& reference;

    NodeIterator() = default;

    NodeIterator(const Node& current, const char* name)
        : current_(current)
        , name_(name)
    {
    }

    const Node& operator*() const { return current_; }
    const Node* operator->() const { return &current_; }

    NodeIterator& operator++()
    {
        current_ =
            name_ ? current_.next_sibling(name_) : current_.next_element();
        return *this;
    }
    NodeIterator operator++(int)
    {
        NodeIterator tmp(*this);
        ++(*this);
        return tmp;
    }

    // Compares position only. Two iterators over different names that have both
    // run out are equal, which is what makes a default-constructed end sentinel
    // work.
    bool operator==(const NodeIterator& rh) const
    {
        return current_ == rh.current_;
    }
    bool operator!=(const NodeIterator& rh) const
    {
        return current_ != rh.current_;
    }

private:
    Node current_;
    const char* name_ = nullptr;
};

// A begin/end pair, so children() composes with range-for and the standard
// algorithms. This is the ergonomic property pugixml was chosen for.
class NodeRange
{
public:
    NodeRange(const Node& first, const char* name)
        : first_(first)
        , name_(name)
    {
    }

    NodeIterator begin() const { return NodeIterator(first_, name_); }
    NodeIterator end() const { return NodeIterator(Node(), name_); }

private:
    Node first_;
    const char* name_;
};

// All element children, in document order.
inline NodeRange Node::children() const
{
    return NodeRange(first_element(), nullptr);
}

// Element children with this name. The name must outlive the range.
inline NodeRange Node::children(const char* name) const
{
    return NodeRange(child(name), name);
}

class Document
{
public:
    Document();
    ~Document();

    // Movable, not copyable: it owns the parsed tree, and pugixml's own
    // xml_document is non-copyable for the same reason. Moving invalidates
    // every Node taken from the moved-from document.
    Document(Document&& rh) noexcept;
    Document& operator=(Document&& rh) noexcept;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // The single outermost element -- <TextureAtlas> for a Sparrow atlas, <map>
    // for a TMX file. Null node for an empty document.
    Node document_element() const;

    // Named child of the document node, so `doc.child("TextureAtlas")` reads
    // the same as pugixml.
    Node child(const char* name) const;

    NodeRange children() const;
    NodeRange children(const char* name) const;

private:
    friend Document parse(const std::string& text);
    friend Document load(const std::string& path);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Parse from memory. Throws ParseError, with pugixml's description and the byte
// offset, for anything malformed.
Document parse(const std::string& text);

// Read a file through util::File and parse it. A missing or unreadable file
// throws the matching posix:: exception rather than a ParseError, since that is
// an I/O failure and not a document one; a malformed document throws ParseError
// naming the path.
Document load(const std::string& path);

} // namespace util::xml
