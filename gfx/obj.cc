#include <GL/glew.h>
#include <gfx/obj.hpp>

namespace gfx
{

using math::Vector3;

ObjModel::ObjModel()
    :  vertices(),
       colors(),
       indexes(),
       vertex_buffer(0),
       color_buffer(0),
       index_buffer(0)
{
}

ObjModel::~ObjModel()
{
    if (vertex_buffer) {
        glDeleteBuffers(1, &vertex_buffer);
        vertex_buffer = 0;
    }
    if (color_buffer) {
        glDeleteBuffers(1, &color_buffer);
        color_buffer = 0;
    }
    if (index_buffer) {
        glDeleteBuffers(1, &index_buffer);
        index_buffer = 0;
    }
}

void ObjModel::add_vertex(const Vector3& v, const Vector3& c)
{
    vertices.push_back(v);
    colors.push_back(c);
}

void ObjModel::add_vertex(const Vector3& v)
{
    add_vertex(v, Vector3(1.0, 1.0, 1.0));
}

void ObjModel::add_vertex(double x, double y, double z)
{
    add_vertex(Vector3(x, y, z));
}

void ObjModel::add_index(int i)
{
    indexes.push_back(i);
}

void ObjModel::add_face(int v1, int v2, int v3)
{
    add_index(v1);
    add_index(v2);
    add_index(v3);
}

void ObjModel::load(const char* fname)
{
}

void ObjModel::build_vbo()
{
    GLenum usage = GL_STATIC_DRAW;

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * vertices.size(),
                 vertices.data(), usage);

    glGenBuffers(1, &color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * colors.size(),
                 colors.data(), usage);

    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indexes.size(),
                 indexes.data(), usage);
}

void ObjModel::render(float x, float y, float z)
{
    // glFrontFace(GL_CCW);

    // glEnable(GL_LIGHTING);
    // glEnable(GL_LIGHT0);
    // glEnable(GL_AUTO_NORMAL);
    // glEnable(GL_NORMALIZE);

    /////////////////////////////
    /// coloring block
    // float shininess = 15.0f;
    // float diffuseColor[3] = {0.929524f, 0.796542f, 0.178823f};
    // float specularColor[4] = {1.00000f, 0.980392f, 0.549020f, 1.0f};
    //
    // // set specular and shiniess using glMaterial (gold-yellow)
    // glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess); // range 0 ~ 128
    // glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
    //
    // // set ambient and diffuse color using glColorMaterial (gold-yellow)
    // glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    // glColor3fv(diffuseColor);
    /// coloring block end
    /////////////////////////////

    // Camera position
    glPushMatrix();
    glTranslatef(-x, -y, -z);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Load the vertex pointer
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glVertexPointer(3, GL_FLOAT, sizeof(Vector3), 0);

    // Load the color pointer
    glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
    glColorPointer(3, GL_FLOAT, sizeof(Vector3), 0);

    // glDrawArrays(GL_TRIANGLES, 0, vertices.size());

    // Draw the elements
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glPopMatrix();

    // glDisable(GL_LIGHTING);
    // glDisable(GL_LIGHT0);
    // glDisable(GL_AUTO_NORMAL);
    // glDisable(GL_NORMALIZE);
}

// vim: set sts=2 sw=2 expandtab:

} // namespace gfx
