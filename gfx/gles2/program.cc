#include <gfx/gles2/program.hpp>

#include <gfx/gles2/api.hpp>

#include <util/format.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <vector>

namespace gfx
{
namespace gles2
{

namespace
{

const char* stage_name(GLenum stage)
{
    return (stage == GL_VERTEX_SHADER) ? "vertex" : "fragment";
}

// The driver's diagnostic, verbatim. GL reports nothing else about a failed
// compile, so dropping this leaves a black screen and no way to find out why.
std::string shader_log(GLuint shader)
{
    GLint length = 0;
    gl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return "(driver supplied no log)";
    }

    // length includes the NUL, which std::string supplies itself.
    std::vector<char> buffer(static_cast<std::size_t>(length));
    gl::GetShaderInfoLog(shader, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

std::string program_log(GLuint program)
{
    GLint length = 0;
    gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return "(driver supplied no log)";
    }

    std::vector<char> buffer(static_cast<std::size_t>(length));
    gl::GetProgramInfoLog(program, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

// Compiles one stage or throws. Returns a shader the caller owns.
GLuint compile(const std::string& label, GLenum stage,
               const std::string& source)
{
    const GLuint shader = gl::CreateShader(stage);
    if (shader == 0) {
        throw ShaderError(util::format("%s: glCreateShader(%s) returned 0",
                                       label.c_str(), stage_name(stage)));
    }

    const char* text = source.c_str();
    const GLint length = static_cast<GLint>(source.size());
    gl::ShaderSource(shader, 1, &text, &length);
    gl::CompileShader(shader);

    GLint compiled = GL_FALSE;
    gl::GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        const std::string log = shader_log(shader);
        gl::DeleteShader(shader);
        throw ShaderError(util::format("%s: %s shader failed to compile:\n%s",
                                       label.c_str(), stage_name(stage),
                                       log.c_str()));
    }
    return shader;
}

} // namespace

Program::Program(const std::string& label, const std::string& vertex_source,
                 const std::string& fragment_source)
    : _program(0)
    , _label(label)
{
    const GLuint vertex = compile(label, GL_VERTEX_SHADER, vertex_source);

    GLuint fragment = 0;
    try {
        fragment = compile(label, GL_FRAGMENT_SHADER, fragment_source);
    } catch (...) {
        // The vertex shader is already ours; a throw from here would leak it.
        gl::DeleteShader(vertex);
        throw;
    }

    _program = gl::CreateProgram();
    if (_program == 0) {
        gl::DeleteShader(vertex);
        gl::DeleteShader(fragment);
        throw ShaderError(
            util::format("%s: glCreateProgram returned 0", label.c_str()));
    }

    gl::AttachShader(_program, vertex);
    gl::AttachShader(_program, fragment);
    gl::LinkProgram(_program);

    // Detached and deleted either way: once linked, the program holds what it
    // needs, and the shader objects are reference-counted by GL so this is the
    // point at which they stop being ours.
    gl::DetachShader(_program, vertex);
    gl::DetachShader(_program, fragment);
    gl::DeleteShader(vertex);
    gl::DeleteShader(fragment);

    GLint linked = GL_FALSE;
    gl::GetProgramiv(_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        const std::string log = program_log(_program);
        gl::DeleteProgram(_program);
        _program = 0;
        throw ShaderError(
            util::format("%s: link failed:\n%s", label.c_str(), log.c_str()));
    }
}

Program::~Program()
{
    if (_program != 0) {
        gl::DeleteProgram(_program);
    }
}

Program::Program(Program&& rh) noexcept
    : _program(rh._program)
    , _label(std::move(rh._label))
{
    rh._program = 0;
}

Program& Program::operator=(Program&& rh) noexcept
{
    if (this != &rh) {
        if (_program != 0) {
            gl::DeleteProgram(_program);
        }
        _program = rh._program;
        _label = std::move(rh._label);
        rh._program = 0;
    }
    return *this;
}

void Program::use() const
{
    gl::UseProgram(_program);
}

int Program::uniform_location(const char* name) const
{
    return gl::GetUniformLocation(_program, name);
}

int Program::attribute_location(const char* name) const
{
    return gl::GetAttribLocation(_program, name);
}

void Program::set_uniform(const char* name, const glm::mat4& value) const
{
    const int location = uniform_location(name);
    if (location < 0) {
        // Not an error: GL reports -1 for a uniform the compiler removed
        // because nothing read it, which happens routinely while a shader is
        // being developed.
        return;
    }
    // GL_FALSE for transpose: glm is column-major already, which is the layout
    // GL wants, so value_ptr can be handed over untouched.
    gl::UniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Program::set_uniform(const char* name, int value) const
{
    const int location = uniform_location(name);
    if (location < 0) {
        return;
    }
    gl::Uniform1i(location, value);
}

} // namespace gles2
} // namespace gfx
