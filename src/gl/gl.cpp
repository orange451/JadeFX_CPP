#include "gl.hpp"

#include <cstdio>

const GLubyte* (*glad_glGetString)(GLenum) = nullptr;
GLenum (*glad_glGetError)() = nullptr;
void (*glad_glClear)(GLbitfield) = nullptr;
void (*glad_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glad_glViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
GLuint (*glad_glCreateShader)(GLenum) = nullptr;
void (*glad_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*glad_glCompileShader)(GLuint) = nullptr;
void (*glad_glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glad_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glad_glDeleteShader)(GLuint) = nullptr;
GLuint (*glad_glCreateProgram)() = nullptr;
void (*glad_glAttachShader)(GLuint, GLuint) = nullptr;
void (*glad_glLinkProgram)(GLuint) = nullptr;
void (*glad_glDeleteProgram)(GLuint) = nullptr;
void (*glad_glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*glad_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*glad_glUseProgram)(GLuint) = nullptr;
void (*glad_glGenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*glad_glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*glad_glBindVertexArray)(GLuint) = nullptr;
void (*glad_glGenBuffers)(GLsizei, GLuint*) = nullptr;
void (*glad_glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*glad_glBindBuffer)(GLenum, GLuint) = nullptr;
void (*glad_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*glad_glEnableVertexAttribArray)(GLuint) = nullptr;
void (*glad_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*glad_glDrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (*glad_glGenTextures)(GLsizei, GLuint*) = nullptr;
void (*glad_glDeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*glad_glBindTexture)(GLenum, GLuint) = nullptr;
void (*glad_glTexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*glad_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*glad_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
void (*glad_glPixelStorei)(GLenum, GLint) = nullptr;
GLint (*glad_glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*glad_glUniform1f)(GLint, GLfloat) = nullptr;
void (*glad_glUniform1i)(GLint, GLint) = nullptr;
void (*glad_glUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
void (*glad_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*glad_glEnable)(GLenum) = nullptr;
void (*glad_glDisable)(GLenum) = nullptr;
void (*glad_glBlendFunc)(GLenum, GLenum) = nullptr;
void (*glad_glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
void (*glad_glScissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;

bool load_gl(GlGetProcAddress get_proc) {
    if (get_proc == nullptr) {
        std::fprintf(stderr, "No OpenGL entry-point loader.\n");
        return false;
    }

#define LOAD(suffix)                                                                 \
    do {                                                                             \
        glad_gl##suffix =                                                            \
            reinterpret_cast<decltype(glad_gl##suffix)>(get_proc("gl" #suffix));     \
        if (glad_gl##suffix == nullptr) {                                            \
            std::fprintf(stderr, "OpenGL entry point gl" #suffix " is unavailable.\n"); \
            return false;                                                            \
        }                                                                            \
    } while (0)

    LOAD(GetString);
    LOAD(GetError);
    LOAD(Clear);
    LOAD(ClearColor);
    LOAD(Viewport);
    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);
    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(LinkProgram);
    LOAD(DeleteProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(UseProgram);
    LOAD(GenVertexArrays);
    LOAD(DeleteVertexArrays);
    LOAD(BindVertexArray);
    LOAD(GenBuffers);
    LOAD(DeleteBuffers);
    LOAD(BindBuffer);
    LOAD(BufferData);
    LOAD(EnableVertexAttribArray);
    LOAD(VertexAttribPointer);
    LOAD(DrawArrays);
    LOAD(GenTextures);
    LOAD(DeleteTextures);
    LOAD(BindTexture);
    LOAD(TexParameteri);
    LOAD(TexImage2D);
    LOAD(TexSubImage2D);
    LOAD(PixelStorei);
    LOAD(GetUniformLocation);
    LOAD(Uniform1f);
    LOAD(Uniform1i);
    LOAD(Uniform2f);
    LOAD(Uniform4f);
    LOAD(Enable);
    LOAD(Disable);
    LOAD(BlendFunc);
    LOAD(ReadPixels);
    LOAD(Scissor);

#undef LOAD
    return true;
}
