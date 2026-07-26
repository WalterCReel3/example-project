#pragma once

#include <vector>

#include <glm/vec3.hpp>

//============================================================================
//
// Renderer-neutral geometry types
//
// No GL, no SDL, no rendering calls. This is what a loader produces and what a
// renderer uploads, and keeping the two apart is what lets loaders/obj.cc — a
// text parser — stop including SDL_opengl.h.
//
// glm rather than the old math::Vector3, which is retired: its operator+ and
// operator* mutated their left operand and returned a reference, so `a + b`
// modified `a` (D7). Reasoning for taking glm in
// planning/2026-07-26-gfx-renderer-and-gles2/ and
// docs/TARGETS.md § "Pinned dependencies".
//
//============================================================================
namespace gfx
{

typedef std::vector<glm::vec3> Vertices;
typedef std::vector<glm::vec3> Colors;
typedef std::vector<unsigned int> Indexes;

// Plain geometry: vertices, per-vertex colours, and triangle indexes into both.
//
// Deliberately an aggregate with no invariant. A Mesh that has been validated
// is not a different type from one that has not, and there is nothing here to
// maintain — the renderer is what decides whether it can draw this.
//
// GPU residency is a separate object with a separate lifetime:
// gfx::gles2::MeshBuffer holds the buffer handles. The 2016 gfx::ObjModel
// merged the two, which is why a text parser depended on OpenGL and why the OBJ
// loader could not be built or tested on a target with no GPU.
struct Mesh {
    Vertices vertices;
    Colors colors;
    Indexes indexes;

    // Indexes address vertices; a mesh whose indexes point past the end of the
    // vertex list reads out of bounds in a draw call, where it presents as
    // corrupt geometry or a GPU fault rather than as a load error. Cheap to
    // check once at load time; O(indexes).
    bool indexes_in_range() const
    {
        for (unsigned int index : indexes) {
            if (index >= vertices.size()) {
                return false;
            }
        }
        return true;
    }

    // Triangles only. A trailing partial triangle means the index list was
    // truncated or the face parser emitted the wrong count.
    bool triangulated() const { return (indexes.size() % 3) == 0; }
};

} // namespace gfx
