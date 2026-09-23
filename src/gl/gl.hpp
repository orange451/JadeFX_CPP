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

// Prefixed so a host's own GL loader can link beside this static library.
extern const GLubyte* (*jadefx_glGetString)(GLenum name);
extern GLenum (*jadefx_glGetError)();
extern void (*jadefx_glClear)(GLbitfield mask);
extern void (*jadefx_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (*jadefx_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
extern GLuint (*jadefx_glCreateShader)(GLenum type);
extern void (*jadefx_glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
extern void (*jadefx_glCompileShader)(GLuint shader);
extern void (*jadefx_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern void (*jadefx_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (*jadefx_glDeleteShader)(GLuint shader);
extern GLuint (*jadefx_glCreateProgram)();
extern void (*jadefx_glAttachShader)(GLuint program, GLuint shader);
extern void (*jadefx_glLinkProgram)(GLuint program);
extern void (*jadefx_glDeleteProgram)(GLuint program);
extern void (*jadefx_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern void (*jadefx_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void (*jadefx_glUseProgram)(GLuint program);
extern void (*jadefx_glGenVertexArrays)(GLsizei n, GLuint* arrays);
extern void (*jadefx_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern void (*jadefx_glBindVertexArray)(GLuint array);
extern void (*jadefx_glGenBuffers)(GLsizei n, GLuint* buffers);
extern void (*jadefx_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
extern void (*jadefx_glBindBuffer)(GLenum target, GLuint buffer);
extern void (*jadefx_glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern void (*jadefx_glEnableVertexAttribArray)(GLuint index);
extern void (*jadefx_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern void (*jadefx_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (*jadefx_glGenTextures)(GLsizei n, GLuint* textures);
extern void (*jadefx_glDeleteTextures)(GLsizei n, const GLuint* textures);
extern void (*jadefx_glBindTexture)(GLenum target, GLuint texture);
extern void (*jadefx_glTexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (*jadefx_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data);
extern void (*jadefx_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
extern void (*jadefx_glPixelStorei)(GLenum pname, GLint param);
extern GLint (*jadefx_glGetUniformLocation)(GLuint program, const GLchar* name);
extern void (*jadefx_glUniform1f)(GLint location, GLfloat v0);
extern void (*jadefx_glUniform1i)(GLint location, GLint v0);
extern void (*jadefx_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (*jadefx_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (*jadefx_glEnable)(GLenum cap);
extern void (*jadefx_glDisable)(GLenum cap);
extern void (*jadefx_glBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (*jadefx_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
extern void (*jadefx_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

#define glGetString jadefx_glGetString
#define glGetError jadefx_glGetError
#define glClear jadefx_glClear
#define glClearColor jadefx_glClearColor
#define glViewport jadefx_glViewport
#define glCreateShader jadefx_glCreateShader
#define glShaderSource jadefx_glShaderSource
#define glCompileShader jadefx_glCompileShader
#define glGetShaderiv jadefx_glGetShaderiv
#define glGetShaderInfoLog jadefx_glGetShaderInfoLog
#define glDeleteShader jadefx_glDeleteShader
#define glCreateProgram jadefx_glCreateProgram
#define glAttachShader jadefx_glAttachShader
#define glLinkProgram jadefx_glLinkProgram
#define glDeleteProgram jadefx_glDeleteProgram
#define glGetProgramiv jadefx_glGetProgramiv
#define glGetProgramInfoLog jadefx_glGetProgramInfoLog
#define glUseProgram jadefx_glUseProgram
#define glGenVertexArrays jadefx_glGenVertexArrays
#define glDeleteVertexArrays jadefx_glDeleteVertexArrays
#define glBindVertexArray jadefx_glBindVertexArray
#define glGenBuffers jadefx_glGenBuffers
#define glDeleteBuffers jadefx_glDeleteBuffers
#define glBindBuffer jadefx_glBindBuffer
#define glBufferData jadefx_glBufferData
#define glEnableVertexAttribArray jadefx_glEnableVertexAttribArray
#define glVertexAttribPointer jadefx_glVertexAttribPointer
#define glDrawArrays jadefx_glDrawArrays
#define glGenTextures jadefx_glGenTextures
#define glDeleteTextures jadefx_glDeleteTextures
#define glBindTexture jadefx_glBindTexture
#define glTexParameteri jadefx_glTexParameteri
#define glTexImage2D jadefx_glTexImage2D
#define glTexSubImage2D jadefx_glTexSubImage2D
#define glPixelStorei jadefx_glPixelStorei
#define glGetUniformLocation jadefx_glGetUniformLocation
#define glUniform1f jadefx_glUniform1f
#define glUniform1i jadefx_glUniform1i
#define glUniform2f jadefx_glUniform2f
#define glUniform4f jadefx_glUniform4f
#define glEnable jadefx_glEnable
#define glDisable jadefx_glDisable
#define glBlendFunc jadefx_glBlendFunc
#define glReadPixels jadefx_glReadPixels
#define glScissor jadefx_glScissor

using GlGetProcAddress = void* (*)(const char* name);

bool jadefx_load_gl(GlGetProcAddress get_proc);
