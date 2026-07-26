#include <gfx/gles2/mesh_buffer.hpp>

#include <gfx/gles2/api.hpp>
#include <gfx/gles2/program.hpp>

#include <util/format.hpp>

#include <stdexcept>
#include <vector>

namespace gfx
{
namespace gles2
{

MeshBuffer::MeshBuffer(const Mesh& mesh)
    : _vertex_buffer(0)
    , _color_buffer(0)
    , _index_buffer(0)
    , _vertex_count(mesh.vertices.size())
    , _index_count(mesh.indexes.size())
{
    // Checked here rather than trusted, because every one of these presents in
    // a draw call as corrupt geometry or a GPU fault, a long way from the
    // cause.
    if (mesh.vertices.empty() || mesh.indexes.empty()) {
        throw std::runtime_error("MeshBuffer: mesh has no geometry");
    }
    if (mesh.colors.size() != mesh.vertices.size()) {
        throw std::runtime_error(
            util::format("MeshBuffer: %zu vertices but %zu colours",
                         mesh.vertices.size(), mesh.colors.size()));
    }
    if (!mesh.triangulated()) {
        throw std::runtime_error(util::format(
            "MeshBuffer: %zu indexes is not a whole number of triangles",
            mesh.indexes.size()));
    }
    if (!mesh.indexes_in_range()) {
        throw std::runtime_error(
            "MeshBuffer: an index addresses a vertex the mesh does not have");
    }

    // GLES 2.0 core indexes with GL_UNSIGNED_SHORT; GL_UNSIGNED_INT needs the
    // OES_element_index_uint extension, which Mali has but which is not
    // guaranteed. Rejecting a mesh too large for 16-bit indexes is honest;
    // silently truncating is not. data/teapot.obj is 3644 vertices, so this is
    // a long way from binding today.
    if (mesh.vertices.size() > 65535) {
        throw std::runtime_error(util::format(
            "MeshBuffer: %zu vertices exceeds the 65535 that GLES 2.0 core can "
            "index; splitting the mesh or requiring OES_element_index_uint "
            "would "
            "be the fix",
            mesh.vertices.size()));
    }

    std::vector<unsigned short> indexes;
    indexes.reserve(mesh.indexes.size());
    for (unsigned int index : mesh.indexes) {
        indexes.push_back(static_cast<unsigned short>(index));
    }

    gl::GenBuffers(1, &_vertex_buffer);
    gl::GenBuffers(1, &_color_buffer);
    gl::GenBuffers(1, &_index_buffer);
    if (_vertex_buffer == 0 || _color_buffer == 0 || _index_buffer == 0) {
        release();
        throw std::runtime_error("MeshBuffer: glGenBuffers produced no name");
    }

    const GLsizeiptr vertex_bytes =
        static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(glm::vec3));

    gl::BindBuffer(GL_ARRAY_BUFFER, _vertex_buffer);
    gl::BufferData(GL_ARRAY_BUFFER, vertex_bytes, mesh.vertices.data(),
                   GL_STATIC_DRAW);

    gl::BindBuffer(GL_ARRAY_BUFFER, _color_buffer);
    gl::BufferData(GL_ARRAY_BUFFER, vertex_bytes, mesh.colors.data(),
                   GL_STATIC_DRAW);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _index_buffer);
    gl::BufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indexes.size() * sizeof(unsigned short)),
        indexes.data(), GL_STATIC_DRAW);

    gl::BindBuffer(GL_ARRAY_BUFFER, 0);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

MeshBuffer::~MeshBuffer()
{
    release();
}

void MeshBuffer::release()
{
    // Guarded because a partially constructed buffer calls this too, and
    // glDeleteBuffers(0) is legal but pointless.
    if (_vertex_buffer != 0) {
        gl::DeleteBuffers(1, &_vertex_buffer);
        _vertex_buffer = 0;
    }
    if (_color_buffer != 0) {
        gl::DeleteBuffers(1, &_color_buffer);
        _color_buffer = 0;
    }
    if (_index_buffer != 0) {
        gl::DeleteBuffers(1, &_index_buffer);
        _index_buffer = 0;
    }
}

MeshBuffer::MeshBuffer(MeshBuffer&& rh) noexcept
    : _vertex_buffer(rh._vertex_buffer)
    , _color_buffer(rh._color_buffer)
    , _index_buffer(rh._index_buffer)
    , _vertex_count(rh._vertex_count)
    , _index_count(rh._index_count)
{
    rh._vertex_buffer = 0;
    rh._color_buffer = 0;
    rh._index_buffer = 0;
    rh._vertex_count = 0;
    rh._index_count = 0;
}

MeshBuffer& MeshBuffer::operator=(MeshBuffer&& rh) noexcept
{
    if (this != &rh) {
        release();
        _vertex_buffer = rh._vertex_buffer;
        _color_buffer = rh._color_buffer;
        _index_buffer = rh._index_buffer;
        _vertex_count = rh._vertex_count;
        _index_count = rh._index_count;
        rh._vertex_buffer = 0;
        rh._color_buffer = 0;
        rh._index_buffer = 0;
        rh._vertex_count = 0;
        rh._index_count = 0;
    }
    return *this;
}

void MeshBuffer::draw(const Program& program, const char* position_attribute,
                      const char* color_attribute) const
{
    const int position = program.attribute_location(position_attribute);
    const int color = program.attribute_location(color_attribute);
    if (position < 0) {
        // Without positions there is nothing to draw, and GL would happily
        // render nothing at all rather than say so.
        return;
    }

    const GLuint position_index = static_cast<GLuint>(position);

    gl::BindBuffer(GL_ARRAY_BUFFER, _vertex_buffer);
    gl::EnableVertexAttribArray(position_index);
    gl::VertexAttribPointer(position_index, 3, GL_FLOAT, GL_FALSE,
                            sizeof(glm::vec3), nullptr);

    // A program that does not read colours is legitimate — a solid-colour or
    // textured shader — so this half is optional.
    const bool has_color = (color >= 0);
    const GLuint color_index = has_color ? static_cast<GLuint>(color) : 0;
    if (has_color) {
        gl::BindBuffer(GL_ARRAY_BUFFER, _color_buffer);
        gl::EnableVertexAttribArray(color_index);
        gl::VertexAttribPointer(color_index, 3, GL_FLOAT, GL_FALSE,
                                sizeof(glm::vec3), nullptr);
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _index_buffer);
    gl::DrawElements(GL_TRIANGLES, static_cast<GLsizei>(_index_count),
                     GL_UNSIGNED_SHORT, nullptr);

    // Left as we found it. Leaking enabled attribute arrays across draws is the
    // classic source of a second object rendering with the first one's data.
    gl::DisableVertexAttribArray(position_index);
    if (has_color) {
        gl::DisableVertexAttribArray(color_index);
    }
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace gles2
} // namespace gfx
