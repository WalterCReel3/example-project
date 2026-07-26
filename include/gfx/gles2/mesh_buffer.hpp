#pragma once

#include <gfx/types.hpp>

namespace gfx
{
namespace gles2
{

class Program;

//============================================================================
//
// GPU residency for a gfx::Mesh
//
// This is the other half of the split that stage 2 made: gfx::Mesh is the data,
// MeshBuffer is the vertex/colour/index buffers holding a copy of it on the
// GPU. The 2016 gfx::ObjModel was both at once, which is why a text parser
// depended on OpenGL and why the loader had no tests.
//
// A MeshBuffer does not reference its Mesh after construction — the data is
// uploaded and the CPU copy can go. That is deliberate: on a 128 MB device,
// keeping vertex data resident twice for no reason is not free.
//
//============================================================================
class MeshBuffer
{
public:
    // Uploads immediately, so a GL context must be current. Throws
    // std::runtime_error for an empty or malformed mesh rather than uploading
    // something that will fault in a draw call.
    explicit MeshBuffer(const Mesh& mesh);
    ~MeshBuffer();

    MeshBuffer(MeshBuffer&& rh) noexcept;
    MeshBuffer& operator=(MeshBuffer&& rh) noexcept;
    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;

    // Binds the buffers, points the named attributes at them, draws, and
    // unbinds. Attribute names rather than fixed locations because GLSL ES 1.00
    // has no layout qualifiers — the program decides its own indices and the
    // caller has to ask.
    //
    // The program must already be in use() and have its uniforms set: this
    // draws geometry, it does not manage state that belongs to the caller's
    // frame.
    void draw(const Program& program, const char* position_attribute,
              const char* color_attribute) const;

    std::size_t vertex_count() const { return _vertex_count; }
    std::size_t index_count() const { return _index_count; }

private:
    void release();

    unsigned int _vertex_buffer;
    unsigned int _color_buffer;
    unsigned int _index_buffer;
    std::size_t _vertex_count;
    std::size_t _index_count;
};

} // namespace gles2
} // namespace gfx
