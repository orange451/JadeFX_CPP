#pragma once

#include <cstddef>
#include <string>

namespace jadefx {

// The shader source with "#define name" on a line of its own, so one file can
// be compiled with and without an #ifdef'd feature. GLSL needs #version before
// anything else, so the define goes on the line right after the first line
// that starts with #version (leading spaces and tabs allowed). That line's end
// is kept as it is, \n or \r\n; the define itself ends in \n, which GLSL reads
// as a line end either way. Without a #version line, the define comes first.
inline std::string WithShaderDefine(const std::string& source, const std::string& name) {
    const std::string define = "#define " + name + "\n";
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        const std::size_t newline = source.find('\n', lineStart);
        const std::size_t lineEnd = newline == std::string::npos ? source.size() : newline;
        const std::size_t first = source.find_first_not_of(" \t", lineStart);
        if (first != std::string::npos && first < lineEnd && source.compare(first, 8, "#version") == 0) {
            if (newline == std::string::npos) {
                // #version is the last line, with no line end to put the define after.
                return source + "\n" + define;
            }
            std::string result = source;
            result.insert(newline + 1, define);
            return result;
        }
        if (newline == std::string::npos) {
            break;
        }
        lineStart = newline + 1;
    }
    return define + source;
}

}  // namespace jadefx
