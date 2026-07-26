#pragma once

#include <stdexcept>
#include <string>

#include <glm/mat4x4.hpp>

#include <util/nocopy.hpp>

//============================================================================
//
// Shaders and programs, GLSL ES 1.00
//
// The replacement for the fixed-function pipeline: instead of glRotatef pushing
// onto a driver-side matrix stack, matrices are built in code with glm and
// handed to a uniform.
//
// A compile or link failure throws ShaderError carrying the driver's info log
// verbatim. That is the whole point of this class existing rather than four
// loose calls: GL reports shader failures only through glGetShaderInfoLog, and
// code that does not read it renders a black screen with no other symptom.
// There is no recovering from a broken shader at runtime, so it is an exception
// rather than a return code.
//
// Written to `#version 100` with explicit precision qualifiers, because GLSL ES
// requires a precision for every float in a fragment shader and desktop GLSL
// does not — a shader that omits it compiles on the dev box and fails on the
// device.
//
//============================================================================
namespace gfx
{
namespace gles2
{

class ShaderError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// A linked vertex + fragment program. Movable so it can be held by value in a
// renderer or a demo; not copyable, because it owns a GL object.
class Program
{
public:
    // Compiles both stages, links, and throws ShaderError with the info log if
    // either step fails. The label appears in those messages and is the only
    // way to tell which of several programs failed.
    Program(const std::string& label, const std::string& vertex_source,
            const std::string& fragment_source);
    ~Program();

    Program(Program&& rh) noexcept;
    Program& operator=(Program&& rh) noexcept;
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    void use() const;

    // -1 when the name is not an active uniform or attribute, which GL also
    // returns when a declared uniform was optimised out for being unused. That
    // is not an error, so it is not thrown: setting a uniform at -1 is a no-op.
    int uniform_location(const char* name) const;
    int attribute_location(const char* name) const;

    void set_uniform(const char* name, const glm::mat4& value) const;
    void set_uniform(const char* name, int value) const;

    unsigned int id() const { return _program; }

private:
    unsigned int _program;
    std::string _label;
};

} // namespace gles2
} // namespace gfx
