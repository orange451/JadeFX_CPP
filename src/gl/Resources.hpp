#pragma once

#include "gl.hpp"

#include <string>

// Shader files live in shaders/. A Mac app also looks in Contents/Resources.
// If the file is missing (a phone package, for example) the embedded copy is used.
std::string LoadShaderSource(const char* filename);

GLuint LinkShaderProgram(const std::string& vertexSource, const std::string& fragmentSource, const char* name);

const char* EmbeddedShader(const char* filename);
