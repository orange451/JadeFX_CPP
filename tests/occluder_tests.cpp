#include "gl/Occluder.hpp"
#include "gl/ShaderDefine.hpp"
#include "gl/UiRenderer.hpp"

#include <cstdio>
#include <string>

// The occluder UiRenderer hides UI fragments behind: its state, which a draw
// reads, kept without a GL context; and the define that builds a shader's
// occluded variant from the same source.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void TestStartsInactive() {
    jadefx::UiRenderer renderer;
    Expect(!renderer.occluder().active(), "a renderer starts with no occluder");
}

void TestSetAndClear() {
    jadefx::UiRenderer renderer;
    const unsigned before = renderer.occluder().revision;
    renderer.setOccluder(7, 10, 20, 300, 200, 0.5f);
    const jadefx::Occluder& set = renderer.occluder();
    Expect(set.active(), "a texture and a rectangle make it active");
    Expect(set.texture == 7 && set.rect[0] == 10.f && set.rect[1] == 20.f && set.rect[2] == 300.f &&
               set.rect[3] == 200.f && set.depth == 0.5f,
           "it keeps what it was given");
    Expect(set.revision != before, "setting it moves the revision");
    const unsigned afterSet = set.revision;
    renderer.setOccluder(7, 10, 20, 300, 200, 0.5f);
    Expect(renderer.occluder().revision == afterSet, "setting the same values again does not");
    renderer.clearOccluder();
    Expect(!renderer.occluder().active(), "clearing it makes it inactive");
    Expect(renderer.occluder().revision != afterSet, "and moves the revision");
}

void TestEmptyIsInactive() {
    jadefx::UiRenderer renderer;
    renderer.setOccluder(0, 0, 0, 100, 100, 0.5f);
    Expect(!renderer.occluder().active(), "no texture is no occluder");
    renderer.setOccluder(3, 0, 0, 0, 100, 0.5f);
    Expect(!renderer.occluder().active(), "nor is an empty rectangle");
}

void TestDefineAfterVersion() {
    const std::string source = "#version 330 core\nout vec4 fragColor;\n";
    Expect(jadefx::WithShaderDefine(source, "JADEFX_OCCLUDER") ==
               "#version 330 core\n#define JADEFX_OCCLUDER\nout vec4 fragColor;\n",
           "the define goes on the line after #version");
}

void TestDefineAfterLeadingComment() {
    const std::string source = "// A comment.\n\n  #version 300 es\nprecision highp float;\n";
    Expect(jadefx::WithShaderDefine(source, "JADEFX_OCCLUDER") ==
               "// A comment.\n\n  #version 300 es\n#define JADEFX_OCCLUDER\nprecision highp float;\n",
           "a comment, a blank line and indentation before #version are passed over");
}

void TestDefineAfterFirstVersionOnly() {
    // Only the first #version line counts; any later one is left alone.
    const std::string source = "#version 330 core\n#version 410\n";
    const std::string result = jadefx::WithShaderDefine(source, "X");
    Expect(result == "#version 330 core\n#define X\n#version 410\n", "only the first #version line is used");
}

void TestDefineWithWindowsLineEnds() {
    const std::string source = "\r\n#version 330 core\r\nout vec4 fragColor;\r\n";
    Expect(jadefx::WithShaderDefine(source, "JADEFX_OCCLUDER") ==
               "\r\n#version 330 core\r\n#define JADEFX_OCCLUDER\nout vec4 fragColor;\r\n",
           "with \\r\\n line ends the define follows the whole #version line end");
}

void TestDefineWithVersionLast() {
    Expect(jadefx::WithShaderDefine("#version 330 core", "X") == "#version 330 core\n#define X\n",
           "a #version line with no line end gets one before the define");
}

void TestDefineWithoutVersion() {
    Expect(jadefx::WithShaderDefine("out vec4 fragColor;\n", "X") == "#define X\nout vec4 fragColor;\n",
           "without #version the define comes first");
    Expect(jadefx::WithShaderDefine("", "X") == "#define X\n", "and an empty source is only the define");
    Expect(jadefx::WithShaderDefine("// #version 330\n", "X") == "#define X\n// #version 330\n",
           "a #version inside a line comment is not the version line");
}

}  // namespace

int RunOccluderTests() {
    TestStartsInactive();
    TestSetAndClear();
    TestEmptyIsInactive();
    TestDefineAfterVersion();
    TestDefineAfterLeadingComment();
    TestDefineAfterFirstVersionOnly();
    TestDefineWithWindowsLineEnds();
    TestDefineWithVersionLast();
    TestDefineWithoutVersion();
    return gFailures;
}
