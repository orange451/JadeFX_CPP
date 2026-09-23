#pragma once

#include "gl.hpp"

#include <string>

// Shader files live in res/shaders. A Mac app looks in Contents/Resources/shaders.
// There is no built-in copy. An empty result means the file could not be read.
std::string LoadShaderSource(const char* filename);

// Directory the running program expects those files in.
std::string ShaderFileDirectory();

GLuint LinkShaderProgram(const std::string& vertexSource, const std::string& fragmentSource, const char* name);
