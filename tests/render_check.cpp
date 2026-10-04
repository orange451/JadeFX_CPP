// By hand, not ctest: jadefx-render-check out-dir draws fixed scenes with
// UiRenderer in a hidden window and writes each as out-dir/<scene>@<scale>x.ppm,
// for a byte comparison against the same program built before a renderer
// change. Shaders are read from the source tree. Then it runs the checks that
// read pixels back, and exits 1 if any write or check failed.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl/UiRenderer.hpp"
#include "gl/gl.hpp"
#include "jadefx/jadefx.hpp"
#include "scene/image/ImageData.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace {

using jadefx::Color;
using jadefx::UiRenderer;

// The window in points. The framebuffer is whatever the display makes of it.
constexpr int kWidth = 480;
constexpr int kHeight = 360;

void* GetProc(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }

Color Rgba(float r, float g, float b, float a) { return Color::rgba(r, g, b, a); }

void Solid(UiRenderer& r) {
    // A grid of cells that share edges, as a table draws them.
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 12; ++col) {
            const float shade = static_cast<float>((row * 12 + col) % 7) / 7.f;
            r.fillRect(10.f + col * 17.5f, 10.f + row * 13.25f, 17.5f, 13.25f, Rgba(shade, 0.4f, 1.f - shade, 1.f));
        }
    }
    // Translucent boxes over each other: their order shows in the blend.
    r.fillRect(40.f, 60.f, 120.f, 80.f, Rgba(1.f, 0.f, 0.f, 0.5f));
    r.fillRect(90.f, 90.f, 120.f, 80.f, Rgba(0.f, 0.f, 1.f, 0.5f));
    r.fillRect(250.3f, 20.7f, 33.4f, 0.6f, Rgba(0.f, 0.f, 0.f, 1.f));
}

void Rounded(UiRenderer& r) {
    const float radii[4][4] = {{0.f, 0.f, 0.f, 0.f}, {6.f, 6.f, 6.f, 6.f}, {20.f, 4.f, 12.f, 0.f}, {40.f, 40.f, 40.f, 40.f}};
    for (int i = 0; i < 4; ++i) {
        const Color fill = Rgba(0.2f + 0.2f * i, 0.5f, 0.3f, 0.8f);
        r.fillRounded(20.f + 110.f * i, 30.f, 90.f, 70.f, radii[i], &fill, nullptr, 1, 0.f);
    }
    const Color glass = Rgba(1.f, 1.f, 1.f, 0.35f);
    const float pill[4] = {15.f, 15.f, 15.f, 15.f};
    r.fillRounded(60.f, 80.f, 300.f, 30.f, pill, &glass, nullptr, 1, 0.f);
}

void Border(UiRenderer& r) {
    const float radius[4] = {8.f, 8.f, 8.f, 8.f};
    const float sharp[4] = {};
    const float even[4] = {1.f, 1.f, 1.f, 1.f};
    const float uneven[4] = {1.f, 3.f, 6.f, 0.5f};
    r.strokeRounded(20.f, 20.f, 140.f, 90.f, radius, even, Rgba(0.f, 0.f, 0.f, 1.f));
    r.strokeRounded(180.f, 20.f, 140.f, 90.f, radius, uneven, Rgba(0.8f, 0.1f, 0.1f, 0.9f));
    r.strokeRounded(20.f, 130.f, 140.f, 90.f, sharp, uneven, Rgba(0.1f, 0.3f, 0.8f, 1.f));
    r.strokeRounded(180.5f, 130.25f, 140.f, 90.f, radius, even, Rgba(0.f, 0.5f, 0.f, 0.5f));
}

void Shadow(UiRenderer& r) {
    const float radius[4] = {10.f, 10.f, 10.f, 10.f};
    const float sharp[4] = {};
    const Color card = Rgba(1.f, 1.f, 1.f, 1.f);
    r.outerShadow(40.f, 40.f, 140.f, 100.f, radius, 0.f, 4.f, 12.f, 0.f, Rgba(0.f, 0.f, 0.f, 0.35f));
    r.fillRounded(40.f, 40.f, 140.f, 100.f, radius, &card, nullptr, 1, 0.f);
    r.outerShadow(240.f, 40.f, 140.f, 100.f, sharp, -3.f, 2.f, 0.f, 4.f, Rgba(0.2f, 0.f, 0.4f, 0.6f));
    r.innerShadow(40.f, 180.f, 140.f, 100.f, radius, 2.f, 2.f, 8.f, 1.f, Rgba(0.f, 0.f, 0.f, 0.5f));
    r.innerShadow(240.f, 180.f, 140.f, 100.f, sharp, 0.f, 0.f, 20.f, -2.f, Rgba(0.9f, 0.2f, 0.f, 0.7f));
}

void Gradient(UiRenderer& r) {
    const float radius[4] = {12.f, 12.f, 12.f, 12.f};
    const Color two[2] = {Rgba(1.f, 0.f, 0.f, 1.f), Rgba(0.f, 0.f, 1.f, 1.f)};
    const float twoAt[2] = {0.f, 1.f};
    const Color three[3] = {Rgba(1.f, 1.f, 0.f, 1.f), Rgba(0.f, 1.f, 0.f, 0.5f), Rgba(0.f, 0.f, 0.f, 1.f)};
    const float threeAt[3] = {0.f, 0.3f, 1.f};
    Color eight[8];
    float eightAt[8];
    for (int i = 0; i < 8; ++i) {
        eight[i] = Rgba(i / 7.f, 1.f - i / 7.f, (i % 2) * 1.f, 1.f);
        eightAt[i] = i / 7.f;
    }
    r.fillRect(10.f, 10.f, 100.f, 60.f, Rgba(0.f, 0.6f, 0.f, 1.f));
    r.fillRounded(30.f, 30.f, 160.f, 90.f, radius, two, twoAt, 2, 90.f);
    // Solid right after a gradient: it must not take the gradient's stops.
    r.fillRect(60.f, 60.f, 100.f, 60.f, Rgba(1.f, 1.f, 0.f, 0.7f));
    r.fillRounded(220.f, 30.f, 160.f, 90.f, radius, three, threeAt, 3, 30.f);
    r.fillRounded(30.f, 160.f, 350.f, 60.f, radius, eight, eightAt, 8, 0.f);
    const float ring[4] = {2.f, 2.f, 2.f, 2.f};
    r.strokeRounded(30.f, 160.f, 350.f, 60.f, radius, ring, Rgba(0.f, 0.f, 0.f, 1.f));
}

std::shared_ptr<jadefx::ImageData> Checker() {
    auto image = std::make_shared<jadefx::ImageData>();
    image->width = 8;
    image->height = 8;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool on = (x + y) % 2 == 0;
            const unsigned char alpha = static_cast<unsigned char>(128 + 16 * x);
            image->rgba.insert(image->rgba.end(), {static_cast<unsigned char>(on ? 255 : 0), 64,
                                                   static_cast<unsigned char>(on ? 0 : 255), alpha});
        }
    }
    return image;
}

void Mixed(UiRenderer& r) {
    static const std::shared_ptr<jadefx::ImageData> image = Checker();
    r.fillRect(10.f, 10.f, 300.f, 40.f, Rgba(0.9f, 0.9f, 0.6f, 1.f));
    r.text(16.f, 18.f, "Boxes under and over text", "Open Sans", 16.f, Rgba(0.f, 0.f, 0.f, 1.f), true);
    r.fillRect(100.f, 20.f, 60.f, 20.f, Rgba(0.f, 0.f, 1.f, 0.4f));
    r.drawImage(image, 20.f, 70.f, 64.f, 64.f, 1.f);
    r.fillRect(50.f, 100.f, 80.f, 20.f, Rgba(1.f, 0.f, 0.f, 0.5f));
    const Color tint = Rgba(0.f, 0.5f, 0.f, 1.f);
    r.drawImage(image, 120.f, 70.f, 64.f, 64.f, 0.6f, &tint);
    r.fillRect(110.f, 60.f, 10.f, 90.f, Rgba(0.f, 0.f, 0.f, 1.f));
    r.text(200.f, 80.f, "grayscale", "Open Sans", 14.f, Rgba(0.2f, 0.2f, 0.2f, 1.f), false);
    r.fillRect(195.f, 85.f, 90.f, 6.f, Rgba(1.f, 0.f, 1.f, 0.5f));
}

void Clips(UiRenderer& r) {
    // Queued before the clip and crossing its edge: drawn whole.
    r.fillRect(0.f, 0.f, 200.f, 200.f, Rgba(0.8f, 0.8f, 1.f, 1.f));
    r.pushClip(50.f, 50.f, 200.f, 150.f);
    r.fillRect(0.f, 0.f, 400.f, 400.f, Rgba(1.f, 0.8f, 0.8f, 1.f));
    r.pushClip(100.5f, 80.5f, 300.f, 60.f);
    r.fillRect(0.f, 0.f, 400.f, 400.f, Rgba(0.2f, 0.6f, 0.2f, 0.6f));
    r.text(90.f, 90.f, "clipped twice", "Open Sans", 18.f, Rgba(0.f, 0.f, 0.f, 1.f), true);
    r.popClip();
    r.fillRect(40.f, 170.f, 300.f, 20.f, Rgba(0.f, 0.f, 0.f, 0.5f));
    r.popClip();
    r.fillRect(300.f, 250.f, 60.f, 60.f, Rgba(0.f, 0.f, 0.f, 1.f));
}

void Many(UiRenderer& r) {
    // 5,200 boxes with nothing between them: more than one run holds.
    for (int i = 0; i < 5200; ++i) {
        const int col = i % 80;
        const int row = i / 80;
        r.fillRect(4.f + col * 5.5f, 4.f + row * 5.25f, 5.f, 4.75f,
                   Rgba((col % 5) / 5.f, (row % 7) / 7.f, 0.5f, 0.9f));
    }
}

struct SceneEntry {
    const char* name;
    void (*draw)(UiRenderer&);
};

const SceneEntry kScenes[] = {
    {"solid", Solid}, {"rounded", Rounded}, {"border", Border}, {"shadow", Shadow},
    {"gradient", Gradient}, {"mixed", Mixed}, {"clips", Clips}, {"many", Many},
};

const Color kWhite = Color::rgba(1.f, 1.f, 1.f, 1.f);

int Write(UiRenderer& renderer, const std::string& outDir, const char* name, float scale) {
    char path[1024];
    std::snprintf(path, sizeof path, "%s/%s@%gx.ppm", outDir.c_str(), name, static_cast<double>(scale));
    if (!renderer.writePpm(path)) {
        std::printf("FAIL could not write %s\n", path);
        return 1;
    }
    std::printf("wrote %s\n", path);
    return 0;
}

int WriteScenes(UiRenderer& renderer, int fbW, int fbH, const std::string& outDir) {
    const float scales[] = {1.f, 1.5f, 2.f};
    int failures = 0;
    for (const SceneEntry& scene : kScenes) {
        for (float scale : scales) {
            // Twice: the second frame starts from whatever state the first left.
            for (int frame = 0; frame < 2; ++frame) {
                renderer.begin(fbW, fbH, scale, kWhite, true);
                scene.draw(renderer);
                renderer.end();
            }
            failures += Write(renderer, outDir, scene.name, scale);
        }
    }
    // A screenshot taken before end, as JADEFX_DUMP_PPM takes one.
    renderer.begin(fbW, fbH, 1.f, kWhite, true);
    Solid(renderer);
    failures += Write(renderer, outDir, "solid-before-end", 1.f);
    renderer.end();
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::printf("FAIL OpenGL error 0x%x\n", error);
        ++failures;
    }
    return failures;
}

// The framebuffer's RGBA at x, y, counted from the bottom left as GL counts.
std::array<unsigned char, 4> PixelAt(int x, int y) {
    std::array<unsigned char, 4> pixel{};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
}

bool IsBlue(const std::array<unsigned char, 4>& p) { return p[0] < 10 && p[1] < 10 && p[2] > 245; }
bool IsRed(const std::array<unsigned char, 4>& p) { return p[0] > 245 && p[1] < 10 && p[2] < 10; }

// GL of a program's own: clears a square to blue, and leaves the scissor off as it found it.
void ClearSquareBlue(int left, int bottom, int size) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, bottom, size, size);
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

int Report(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    return ok ? 0 : 1;
}

// A stage whose UI fills the window red, then a rendering callback that clears
// a square in the middle blue. The callback runs after the UI, so blue is on top.
int CheckRenderingCallback(int fbW, int fbH) {
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&GetProc)) {
        return Report(false, "a rendering callback draws over the UI (no graphics)");
    }
    auto root = jadefx::make<jadefx::StackPane>();
    root->setStyle("background-color: #ff0000;");
    // Empty, it would lay out at its preferred size, none.
    root->setMinSize(kWidth, kHeight);
    stage.getScene().setRoot(root);
    const int size = fbH / 4;
    stage.setRenderingCallback([&](int, int) { ClearSquareBlue(fbW / 2 - size / 2, fbH / 2 - size / 2, size); });
    stage.frame(kWidth, kHeight, fbW, fbH);
    const bool onTop = IsBlue(PixelAt(fbW / 2, fbH / 2));
    const bool uiDrawn = IsRed(PixelAt(2, 2));
    stage.shutdownGraphics();
    return Report(onTop && uiDrawn, "a rendering callback draws over the UI");
}

int RunChecks(UiRenderer&, int fbW, int fbH) {
    int failures = 0;
    failures += CheckRenderingCallback(fbW, fbH);
    return failures;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outDir = argc > 1 ? argv[1] : ".";
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "jadefx-render-check", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "No GL window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!jadefx_load_gl(&GetProc)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    jadefx::Font::loadDefault();
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    std::printf("framebuffer %dx%d\n", fbW, fbH);

    int failures = 0;
    {
        UiRenderer renderer;
        if (!renderer.initialize()) {
            std::fprintf(stderr, "UiRenderer did not start\n");
            failures = 1;
        } else {
            failures += WriteScenes(renderer, fbW, fbH, outDir);
            failures += RunChecks(renderer, fbW, fbH);
            renderer.shutdown();
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    if (failures == 0) {
        std::printf("render-check: all passed\n");
        return 0;
    }
    std::printf("render-check: %d failed\n", failures);
    return 1;
}
