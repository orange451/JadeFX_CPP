#pragma once

#include <cstddef>

// Windows ships OpenGL 1.1 in opengl32.dll. macOS and Linux export newer entry
// points from the driver. Load every call after a context exists so the same
// source links on desktop GL and OpenGL ES.

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLubyte = unsigned char;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

#ifdef GL_FALSE
#undef GL_FALSE
#endif
#ifdef GL_TRUE
#undef GL_TRUE
#endif
#ifdef GL_NO_ERROR
#undef GL_NO_ERROR
#endif
#ifdef GL_TRIANGLES
#undef GL_TRIANGLES
#endif
#ifdef GL_COLOR_BUFFER_BIT
#undef GL_COLOR_BUFFER_BIT
#endif
#ifdef GL_VERSION
#undef GL_VERSION
#endif
#ifdef GL_RENDERER
#undef GL_RENDERER
#endif
#ifdef GL_FLOAT
#undef GL_FLOAT
#endif
#ifdef GL_ARRAY_BUFFER
#undef GL_ARRAY_BUFFER
#endif
#ifdef GL_STATIC_DRAW
#undef GL_STATIC_DRAW
#endif
#ifdef GL_DYNAMIC_DRAW
#undef GL_DYNAMIC_DRAW
#endif
#ifdef GL_FRAGMENT_SHADER
#undef GL_FRAGMENT_SHADER
#endif
#ifdef GL_VERTEX_SHADER
#undef GL_VERTEX_SHADER
#endif
#ifdef GL_COMPILE_STATUS
#undef GL_COMPILE_STATUS
#endif
#ifdef GL_LINK_STATUS
#undef GL_LINK_STATUS
#endif
#ifdef GL_TEXTURE_2D
#undef GL_TEXTURE_2D
#endif
#ifdef GL_RED
#undef GL_RED
#endif
#ifdef GL_R8
#undef GL_R8
#endif
#ifdef GL_RGBA
#undef GL_RGBA
#endif
#ifdef GL_RGBA8
#undef GL_RGBA8
#endif
#ifdef GL_SRC1_COLOR
#undef GL_SRC1_COLOR
#endif
#ifdef GL_ONE_MINUS_SRC1_COLOR
#undef GL_ONE_MINUS_SRC1_COLOR
#endif
#ifdef GL_UNSIGNED_BYTE
#undef GL_UNSIGNED_BYTE
#endif
#ifdef GL_TEXTURE_MIN_FILTER
#undef GL_TEXTURE_MIN_FILTER
#endif
#ifdef GL_TEXTURE_MAG_FILTER
#undef GL_TEXTURE_MAG_FILTER
#endif
#ifdef GL_NEAREST
#undef GL_NEAREST
#endif
#ifdef GL_LINEAR
#undef GL_LINEAR
#endif
#ifdef GL_TEXTURE_WRAP_S
#undef GL_TEXTURE_WRAP_S
#endif
#ifdef GL_TEXTURE_WRAP_T
#undef GL_TEXTURE_WRAP_T
#endif
#ifdef GL_CLAMP_TO_EDGE
#undef GL_CLAMP_TO_EDGE
#endif
#ifdef GL_BLEND
#undef GL_BLEND
#endif
#ifdef GL_SRC_ALPHA
#undef GL_SRC_ALPHA
#endif
#ifdef GL_ONE_MINUS_SRC_ALPHA
#undef GL_ONE_MINUS_SRC_ALPHA
#endif
#ifdef GL_UNPACK_ALIGNMENT
#undef GL_UNPACK_ALIGNMENT
#endif
#ifdef GL_PACK_ALIGNMENT
#undef GL_PACK_ALIGNMENT
#endif
#ifdef GL_DEPTH_TEST
#undef GL_DEPTH_TEST
#endif
#ifdef GL_CULL_FACE
#undef GL_CULL_FACE
#endif

constexpr GLboolean GL_FALSE = 0;
constexpr GLboolean GL_TRUE = 1;
constexpr GLenum GL_NO_ERROR = 0;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_VERSION = 0x1F02;
constexpr GLenum GL_RENDERER = 0x1F01;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_RED = 0x1903;
constexpr GLint GL_R8 = 0x8229;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLint GL_RGBA8 = 0x8058;
constexpr GLenum GL_SRC1_COLOR = 0x88F9;
constexpr GLenum GL_ONE_MINUS_SRC1_COLOR = 0x88FA;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;

extern const GLubyte* (*glad_glGetString)(GLenum name);
extern GLenum (*glad_glGetError)();
extern void (*glad_glClear)(GLbitfield mask);
extern void (*glad_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (*glad_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
extern GLuint (*glad_glCreateShader)(GLenum type);
extern void (*glad_glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
extern void (*glad_glCompileShader)(GLuint shader);
extern void (*glad_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern void (*glad_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (*glad_glDeleteShader)(GLuint shader);
extern GLuint (*glad_glCreateProgram)();
extern void (*glad_glAttachShader)(GLuint program, GLuint shader);
extern void (*glad_glLinkProgram)(GLuint program);
extern void (*glad_glDeleteProgram)(GLuint program);
extern void (*glad_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern void (*glad_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (*glad_glUseProgram)(GLuint program);
extern void (*glad_glGenVertexArrays)(GLsizei n, GLuint* arrays);
extern void (*glad_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern void (*glad_glBindVertexArray)(GLuint array);
extern void (*glad_glGenBuffers)(GLsizei n, GLuint* buffers);
extern void (*glad_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
extern void (*glad_glBindBuffer)(GLenum target, GLuint buffer);
extern void (*glad_glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern void (*glad_glEnableVertexAttribArray)(GLuint index);
extern void (*glad_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern void (*glad_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (*glad_glGenTextures)(GLsizei n, GLuint* textures);
extern void (*glad_glDeleteTextures)(GLsizei n, const GLuint* textures);
extern void (*glad_glBindTexture)(GLenum target, GLuint texture);
extern void (*glad_glTexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (*glad_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data);
extern void (*glad_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
extern void (*glad_glPixelStorei)(GLenum pname, GLint param);
extern GLint (*glad_glGetUniformLocation)(GLuint program, const GLchar* name);
extern void (*glad_glUniform1f)(GLint location, GLfloat v0);
extern void (*glad_glUniform1i)(GLint location, GLint v0);
extern void (*glad_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (*glad_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (*glad_glEnable)(GLenum cap);
extern void (*glad_glDisable)(GLenum cap);
extern void (*glad_glBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (*glad_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
extern void (*glad_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

#define glGetString glad_glGetString
#define glGetError glad_glGetError
#define glClear glad_glClear
#define glClearColor glad_glClearColor
#define glViewport glad_glViewport
#define glCreateShader glad_glCreateShader
#define glShaderSource glad_glShaderSource
#define glCompileShader glad_glCompileShader
#define glGetShaderiv glad_glGetShaderiv
#define glGetShaderInfoLog glad_glGetShaderInfoLog
#define glDeleteShader glad_glDeleteShader
#define glCreateProgram glad_glCreateProgram
#define glAttachShader glad_glAttachShader
#define glLinkProgram glad_glLinkProgram
#define glDeleteProgram glad_glDeleteProgram
#define glGetProgramiv glad_glGetProgramiv
#define glGetProgramInfoLog glad_glGetProgramInfoLog
#define glUseProgram glad_glUseProgram
#define glGenVertexArrays glad_glGenVertexArrays
#define glDeleteVertexArrays glad_glDeleteVertexArrays
#define glBindVertexArray glad_glBindVertexArray
#define glGenBuffers glad_glGenBuffers
#define glDeleteBuffers glad_glDeleteBuffers
#define glBindBuffer glad_glBindBuffer
#define glBufferData glad_glBufferData
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glVertexAttribPointer glad_glVertexAttribPointer
#define glDrawArrays glad_glDrawArrays
#define glGenTextures glad_glGenTextures
#define glDeleteTextures glad_glDeleteTextures
#define glBindTexture glad_glBindTexture
#define glTexParameteri glad_glTexParameteri
#define glTexImage2D glad_glTexImage2D
#define glTexSubImage2D glad_glTexSubImage2D
#define glPixelStorei glad_glPixelStorei
#define glGetUniformLocation glad_glGetUniformLocation
#define glUniform1f glad_glUniform1f
#define glUniform1i glad_glUniform1i
#define glUniform2f glad_glUniform2f
#define glUniform4f glad_glUniform4f
#define glEnable glad_glEnable
#define glDisable glad_glDisable
#define glBlendFunc glad_glBlendFunc
#define glReadPixels glad_glReadPixels
#define glScissor glad_glScissor

using GlGetProcAddress = void* (*)(const char* name);

bool load_gl(GlGetProcAddress get_proc);
