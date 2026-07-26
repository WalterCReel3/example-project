#include <loaders/obj.hpp>

#include <util/file.hpp>
#include <util/format.hpp>
#include <util/io.hpp>
#include <util/number.hpp>
#include <util/string.hpp>

#include <string>
#include <vector>

namespace loaders
{

namespace
{

// A face index token is either "12" or "12/3/4" — vertex, texture, normal. Only
// the vertex index is used here, so everything from the first slash is dropped.
// The 2016 parser got this right by accident: strtol stops at '/', so the slash
// form parsed and nothing recorded that it was supported.
std::string vertex_index_field(const std::string& token)
{
    const std::string::size_type slash = token.find('/');
    return (slash == std::string::npos) ? token : token.substr(0, slash);
}

float parse_coordinate(const std::string& token, std::size_t line,
                       const std::string& filename)
{
    float value = 0.0f;
    if (!util::from_string(token, value)) {
        throw ObjFormatError(util::format("%s:%zu: '%s' is not a coordinate",
                                          filename.c_str(), line,
                                          token.c_str()));
    }
    return value;
}

// Returns a 0-based index. OBJ counts from 1.
unsigned int parse_index(const std::string& token, std::size_t line,
                         const std::string& filename)
{
    const std::string field = vertex_index_field(token);

    long value = 0;
    if (!util::from_string(field, value)) {
        throw ObjFormatError(util::format("%s:%zu: '%s' is not a vertex index",
                                          filename.c_str(), line,
                                          token.c_str()));
    }
    if (value < 0) {
        // OBJ allows an index relative to the end of the vertex list. Nothing
        // here implements it, and resolving it wrongly produces geometry that
        // looks plausible from some angles.
        throw ObjFormatError(
            util::format("%s:%zu: end-relative index %ld is not supported",
                         filename.c_str(), line, value));
    }
    if (value == 0) {
        throw ObjFormatError(util::format("%s:%zu: index 0 is invalid; OBJ "
                                          "indexes count from 1",
                                          filename.c_str(), line));
    }
    return static_cast<unsigned int>(value - 1);
}

void parse_vertex(const std::vector<std::string>& tokens, std::size_t line,
                  const std::string& filename, gfx::Mesh& mesh)
{
    if (tokens.size() < 4) {
        throw ObjFormatError(
            util::format("%s:%zu: 'v' needs three coordinates, "
                         "found %zu token(s)",
                         filename.c_str(), line, tokens.size() - 1));
    }

    const float x = parse_coordinate(tokens[1], line, filename);
    const float y = parse_coordinate(tokens[2], line, filename);
    const float z = parse_coordinate(tokens[3], line, filename);

    mesh.vertices.push_back(glm::vec3(x, y, z));

    // A placeholder colour derived from the position, carried over from the
    // 2016 loader so the skratch demo keeps the look it had. OBJ has no
    // per-vertex colour; a real one comes from the material or from a shader.
    mesh.colors.push_back(glm::vec3(1.0f - x, 1.0f - y, 1.0f - z));
}

void parse_face(const std::vector<std::string>& tokens, std::size_t line,
                const std::string& filename, gfx::Mesh& mesh)
{
    if (tokens.size() < 4) {
        throw ObjFormatError(util::format("%s:%zu: 'f' needs at least three "
                                          "vertices, found %zu",
                                          filename.c_str(), line,
                                          tokens.size() - 1));
    }

    // Fan triangulation: (0,1,2), (0,2,3), ... For a triangle this is one
    // triangle and identical to the original's behaviour. For a quad the
    // original silently dropped the fourth vertex, leaving a hole.
    const unsigned int first = parse_index(tokens[1], line, filename);
    unsigned int previous = parse_index(tokens[2], line, filename);

    for (std::size_t i = 3; i < tokens.size(); ++i) {
        const unsigned int current = parse_index(tokens[i], line, filename);
        mesh.indexes.push_back(first);
        mesh.indexes.push_back(previous);
        mesh.indexes.push_back(current);
        previous = current;
    }
}

} // namespace

gfx::Mesh load_obj(const std::string& filename)
{
    typedef util::quoted_whitespace_tokenizer<std::string> tokenizer_t;
    typedef util::token_generator<std::string::const_iterator, tokenizer_t>
        token_gen_t;

    std::string file_buffer;
    util::File input(filename, util::OpenReadOnly);
    util::read_all(input, file_buffer);

    gfx::Mesh mesh;
    std::vector<std::string> tokens;
    std::size_t line = 0;

    util::line_iterator<std::string> li(file_buffer);
    const util::line_iterator<std::string> eos;

    for (; li != eos; ++li) {
        ++line;

        token_gen_t gentok(li->first, li->second, tokenizer_t());
        for (std::string token = gentok(); token.size() != 0;
             token = gentok()) {
            tokens.push_back(token);
        }

        if (!tokens.empty()) {
            const std::string& keyword = tokens[0];
            if (keyword == "v") {
                parse_vertex(tokens, line, filename, mesh);
            } else if (keyword == "f") {
                parse_face(tokens, line, filename, mesh);
            }
            // Everything else is ignored: normals, texture coordinates,
            // materials, groups, comments.
        }
        tokens.clear();
    }

    // Checked after the whole file rather than per face, because OBJ indexes
    // address the file's complete vertex list and a well-formed file may in
    // principle declare a face before the vertices it uses.
    if (!mesh.indexes_in_range()) {
        throw ObjFormatError(util::format(
            "%s: a face indexes a vertex the file does not declare "
            "(%zu vertices, %zu indexes)",
            filename.c_str(), mesh.vertices.size(), mesh.indexes.size()));
    }

    return mesh;
}

} // namespace loaders
