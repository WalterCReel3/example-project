#pragma once

#include <stdexcept>
#include <string>

#include <gfx/types.hpp>

//============================================================================
//
// Wavefront OBJ
//
// A text parser, and now only that. It produces gfx::Mesh — plain vertex,
// colour and index data — so it builds and is testable on every target
// including the GPU-less Miyoo Mini.
//
// It used to fill a gfx::ObjModel, which held GLuint buffer handles alongside
// its vertex data. That made this header include SDL_opengl.h transitively,
// which is why loaders/CMakeLists.txt had to exclude obj.cc unless a GL backend
// was configured, and why the loader had no tests at all. Uploading a Mesh to
// the GPU is gfx::gles2::MeshBuffer's job.
//
// Supported subset:
//
//     v x y z            vertex position
//     f a b c            triangular face, 1-based indexes
//     f a/t/n ...        the same, with texture and normal indexes present.
//                        Only the vertex index is read; OBJ writers emit this
//                        form routinely, so rejecting it would fail on files
//                        that work everywhere else.
//
// Ignored, as the original did: `vn`, `vt`, `usemtl`, `mtllib`, `o`, `s`, `g`,
// comments, and anything else. Ignoring unknown keywords rather than rejecting
// them is deliberate — an OBJ with normals is still a usable mesh.
//
// Rejected rather than mis-parsed:
//
//     - a coordinate or index that is not a number ("1.0.0", "12px", "")
//     - negative (end-relative) indexes, which OBJ permits and this does not
//       implement; silently treating -1 as index 4294967294 is worse than
//       saying so
//     - faces with fewer than three vertices
//     - an index addressing a vertex the file never declares
//
// Not rejected: a face with more than three vertices, which is triangulated as
// a fan. The original took the first three vertices and dropped the rest.
//
//============================================================================
namespace loaders
{

// Errors are types — see include/posix/errors.hpp. A malformed file is an error
// rather than a truncated model, because a mesh that is quietly missing its
// last hundred faces looks like a rendering bug.
class ObjFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Throws ObjFormatError for a malformed file, and the matching posix::
// exception for one that cannot be read.
gfx::Mesh load_obj(const std::string& filename);

} // namespace loaders
