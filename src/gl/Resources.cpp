#include "Resources.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/native_activity.h>

extern "C" ANativeActivity* glfmAndroidGetActivity(void);
#endif

namespace {

namespace fs = std::filesystem;

fs::path ExecutableDirectory() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return fs::path(buffer).parent_path();
#elif defined(__APPLE__)
    std::string buffer(256, '\0');
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.assign(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
    }
    const std::size_t terminator = buffer.find('\0');
    if (terminator == std::string::npos) {
        return {};
    }
    buffer.resize(terminator);
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(buffer, error);
    return (error ? fs::path(buffer) : canonical).parent_path();
#else
    std::string buffer(256, '\0');
    for (;;) {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return {};
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<std::size_t>(length));
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return fs::path(buffer).parent_path();
#endif
}

bool IsFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error);
}

std::string DisplayPath(const fs::path& path) {
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(path, error);
    return (error ? path.lexically_normal() : canonical).u8string();
}

#if defined(__ANDROID__)
std::string ReadAndroidAsset(const char* filename) {
    ANativeActivity* activity = glfmAndroidGetActivity();
    if (activity == nullptr || activity->assetManager == nullptr || filename == nullptr) {
        return {};
    }
    const std::string path = std::string("shaders/") + filename;
    AAsset* asset = AAssetManager_open(activity->assetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        return {};
    }
    const off_t length = AAsset_getLength(asset);
    std::string contents;
    if (length > 0) {
        contents.resize(static_cast<std::size_t>(length));
        const int read = AAsset_read(asset, contents.data(), static_cast<std::size_t>(length));
        if (read != static_cast<int>(length)) {
            contents.clear();
        }
    }
    AAsset_close(asset);
    return contents;
}
#endif

std::string ReadTextFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) {
        return {};
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    file.seekg(0, std::ios::beg);
    if (size == 0) {
        return contents;
    }
    file.read(contents.data(), size);
    if (file.gcount() != size) {
        return {};
    }
    return contents;
}

void PrintShaderLog(GLuint shader, const char* stage) {
    char log[2048];
    glGetShaderInfoLog(shader, sizeof log, nullptr, log);
    std::fprintf(stderr, "%s shader failed to compile:\n%s\n", stage, log);
}

void PrintProgramLog(GLuint program) {
    char log[2048];
    glGetProgramInfoLog(program, sizeof log, nullptr, log);
    std::fprintf(stderr, "Program failed to link:\n%s\n", log);
}

GLuint Compile(GLenum type, const char* source, const char* stage) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        PrintShaderLog(shader, stage);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

std::string Preamble(const char* filename) {
#if defined(JADEFX_GLES)
    std::string header = "#version 300 es\n";
    const bool fragment = filename != nullptr && std::strstr(filename, ".frag") != nullptr;
    // The extension has to precede the precision statement. Desktop GL 3.3 has this in core.
    if (fragment && std::strcmp(filename, "text.frag") == 0) {
        header += "#extension GL_EXT_blend_func_extended : require\n";
    }
    if (fragment) {
        header += "precision highp float;\n";
    }
    return header;
#else
    (void)filename;
    return "#version 330 core\n";
#endif
}

}  // namespace

std::string LoadShaderSource(const char* filename) {
    const fs::path exeDir = ExecutableDirectory();
    fs::path candidates[8];
    std::size_t count = 0;
    if (!exeDir.empty()) {
        candidates[count++] = exeDir / ".." / "Resources" / "shaders" / filename;
        candidates[count++] = exeDir / "shaders" / filename;
        candidates[count++] = exeDir / ".." / "shaders" / filename;
    }
    candidates[count++] = fs::path("shaders") / filename;
#if defined(JADEFX_SOURCE_DIR)
    candidates[count++] = fs::path(JADEFX_SOURCE_DIR) / "shaders" / filename;
#endif

    for (std::size_t i = 0; i < count; ++i) {
        if (!IsFile(candidates[i])) {
            continue;
        }
        std::string source = ReadTextFile(candidates[i]);
        if (!source.empty()) {
            return Preamble(filename) + source;
        }
    }

#if defined(__ANDROID__)
    const std::string asset = ReadAndroidAsset(filename);
    if (!asset.empty()) {
        return Preamble(filename) + asset;
    }
#endif
    std::fprintf(stderr, "Could not find shader file \"%s\".\n", filename);
    return {};
}

std::string ShaderFileDirectory() {
#if defined(__ANDROID__)
    return "assets/shaders";
#else
    const fs::path exeDir = ExecutableDirectory();
#if defined(__APPLE__) && !defined(JADEFX_GLFM)
    const fs::path dir = exeDir.empty() ? fs::path("shaders") : exeDir / ".." / "Resources" / "shaders";
#else
    const fs::path dir = exeDir.empty() ? fs::path("shaders") : exeDir / "shaders";
#endif
    return DisplayPath(dir);
#endif
}

GLuint LinkShaderProgram(const std::string& vertexSource, const std::string& fragmentSource, const char* name) {
    if (vertexSource.empty() || fragmentSource.empty()) {
        return 0;
    }
    const std::string vertexStage = std::string(name) + " vertex";
    const std::string fragmentStage = std::string(name) + " fragment";
    GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource.c_str(), vertexStage.c_str());
    GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource.c_str(), fragmentStage.c_str());
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        PrintProgramLog(program);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
