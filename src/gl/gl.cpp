#include "gl.hpp"

#include <cstdio>

const GLubyte* (*jadefx_glGetString)(GLenum) = nullptr;
GLenum (*jadefx_glGetError)() = nullptr;
void (*jadefx_glClear)(GLbitfield) = nullptr;
void (*jadefx_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*jadefx_glViewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
GLuint (*jadefx_glCreateShader)(GLenum) = nullptr;
void (*jadefx_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*jadefx_glCompileShader)(GLuint) = nullptr;
void (*jadefx_glGetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*jadefx_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*jadefx_glDeleteShader)(GLuint) = nullptr;
GLuint (*jadefx_glCreateProgram)() = nullptr;
void (*jadefx_glAttachShader)(GLuint, GLuint) = nullptr;
void (*jadefx_glLinkProgram)(GLuint) = nullptr;
void (*jadefx_glDeleteProgram)(GLuint) = nullptr;
void (*jadefx_glGetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*jadefx_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*jadefx_glUseProgram)(GLuint) = nullptr;
void (*jadefx_glGenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*jadefx_glDeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*jadefx_glBindVertexArray)(GLuint) = nullptr;
void (*jadefx_glGenBuffers)(GLsizei, GLuint*) = nullptr;
void (*jadefx_glDeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*jadefx_glBindBuffer)(GLenum, GLuint) = nullptr;
void (*jadefx_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*jadefx_glEnableVertexAttribArray)(GLuint) = nullptr;
void (*jadefx_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
void (*jadefx_glDrawArrays)(GLenum, GLint, GLsizei) = nullptr;
void (*jadefx_glGenTextures)(GLsizei, GLuint*) = nullptr;
void (*jadefx_glDeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*jadefx_glBindTexture)(GLenum, GLuint) = nullptr;
void (*jadefx_glTexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*jadefx_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*jadefx_glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
void (*jadefx_glPixelStorei)(GLenum, GLint) = nullptr;
GLint (*jadefx_glGetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*jadefx_glUniform1f)(GLint, GLfloat) = nullptr;
void (*jadefx_glUniform1i)(GLint, GLint) = nullptr;
void (*jadefx_glUniform2f)(GLint, GLfloat, GLfloat) = nullptr;
void (*jadefx_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*jadefx_glEnable)(GLenum) = nullptr;
void (*jadefx_glDisable)(GLenum) = nullptr;
void (*jadefx_glBlendFunc)(GLenum, GLenum) = nullptr;
void (*jadefx_glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
void (*jadefx_glScissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;

bool jadefx_load_gl(GlGetProcAddress get_proc) {
    if (get_proc == nullptr) {
        std::fprintf(stderr, "No OpenGL entry-point loader.\n");
        return false;
    }

#define LOAD(suffix)                                                                 \
    do {                                                                             \
        jadefx_gl##suffix =                                                            \
            reinterpret_cast<decltype(jadefx_gl##suffix)>(get_proc("gl" #suffix));     \
        if (jadefx_gl##suffix == nullptr) {                                            \
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
