#ifndef __GFX_OBJ_HPP__
#define __GFX_OBJ_HPP__

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <SDL_opengl.h>
#endif
#include <gfx/types.hpp>

namespace gfx
{

class ObjModel
{
public:
    ObjModel();
    ~ObjModel();

    void add_vertex(const math::Vector3& v, const math::Vector3& c);
    void add_vertex(const math::Vector3& v);
    void add_vertex(double x, double y, double z);
    void add_face(int v1, int v2, int v3);
    void add_index(int i);
    void load(const char* fname);
    void build_vbo();
    void render(float x, float y, float z);

public:
    Vertices vertices;
    Colors colors;
    Indexes indexes;
    GLuint vertex_buffer;
    GLuint color_buffer;
    GLuint index_buffer;
};

} // namespace gfx

#endif
