#pragma once

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <glm/gtc/matrix_transform.hpp>

//============================================================================
//
// The demo's camera
//
// Replaces gfx::Pov and gfx::Orientation, which went with gl_legacy.
// Orientation derived from a 3-vector so that yaw, pitch and roll could be x, y
// and z behind accessors — a type inheriting a vector to rename its members.
// Here they are three floats called what they are.
//
// This lives in skratch rather than in gfx because it is demo state, not
// renderer state. The renderer takes a matrix; how a program decides on that
// matrix is its own business.
//
// Angles are DEGREES, as the 2016 input code produced them and as glRotatef
// took them. glm's rotate takes radians, so the conversion happens once, here —
// getting that wrong is the classic fixed-function-to-shader porting bug, and
// it looks like a camera that barely moves.
//
//============================================================================
class Camera
{
public:
    // Starts back and above the grid, looking into it. The 2016 demo started at
    // the origin, which is exactly where its first instance is placed — so it
    // opened from inside an icosahedron, with the screen filled by one model's
    // interpolated colours. Faithful, and a poor first frame for a demo whose
    // job is now to show the renderer working.
    glm::vec3 position{25.0f, 25.0f, 30.0f};
    float yaw = -30.0f;
    float pitch = 20.0f;
    float roll = 0.0f;

    // The view matrix, in the same order the fixed-function version applied it:
    //
    //     glRotatef(pitch, 1,0,0);  glRotatef(yaw, 0,1,0);  glRotatef(roll,
    //     0,0,1); glTranslatef(-x, -y, -z);
    //
    // GL post-multiplied each of those onto the model-view matrix, giving
    // R_pitch * R_yaw * R_roll * T(-position). glm::rotate(m, a, axis) also
    // returns m * R, so chaining in the same sequence reproduces it exactly
    // rather than approximately.
    glm::mat4 view() const
    {
        glm::mat4 v(1.0f);
        v = glm::rotate(v, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        v = glm::rotate(v, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        v = glm::rotate(v, glm::radians(roll), glm::vec3(0.0f, 0.0f, 1.0f));
        v = glm::translate(v, -position);
        return v;
    }
};
