#include "jadefx/jadefx.hpp"

#include "gl/gl.hpp"
#include "platform/GlfwHost.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// Painter::drawTexture: an app's own GL texture drawn as a node's content,
// straight or premultiplied, flipped, and faded like an image.
namespace {

int gFailures = 0;

// The window the tests draw in, which the texture fills.
constexpr int kWidth = 240;
constexpr int kHeight = 160;
constexpr std::size_t kFrameBytes = static_cast<std::size_t>(kWidth) * kHeight * 3;

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message.c_str());
        ++gFailures;
    }
}

// A pane that makes a 1 by 2 texture on its first paint, the bottom row (GL's
// row 0) blue and the top row red, both premultiplied at alpha, and draws it
// over its whole box.
class TextureNode : public jadefx::Pane {
public:
    TextureNode(unsigned char alpha, float opacity, jadefx::TextureAlpha mode)
        : alpha_(alpha), opacity_(opacity), mode_(mode) {}

protected:
    void renderContent(jadefx::UiRenderer& renderer, float) override {
        if (texture_ == 0) {
            const unsigned char a = alpha_;
            const unsigned char pixels[] = {0, 0, a, a, a, 0, 0, a};
            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_NEAREST));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_NEAREST));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        }
            jadefx::Painter painter(renderer);
        painter.drawTexture(texture_, static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                            static_cast<float>(getWidth()), static_cast<float>(getHeight()), opacity_, mode_, true);
    }

private:
    unsigned char alpha_;
    float opacity_;
    jadefx::TextureAlpha mode_;
    unsigned texture_ = 0;
};

// Draws root on black, filling the window, twice and returns the last frame's
// RGB, top row first. Empty when the window, the context, or the dump failed;
// ok says whether the frames drew with no GL error.
std::vector<unsigned char> RenderFrame(const std::shared_ptr<jadefx::Node>& root, bool& ok) {
    ok = false;
    jadefx::GlfwHost host;
    if (!host.create(kWidth, kHeight, "texture")) {
        Expect(false, "TX window opens");
        return {};
    }
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        Expect(false, "TX context");
        host.destroy();
        return {};
    }
    root->setPrefSize(kWidth, kHeight);
    root->setStyle("background-color: #000000;");
    stage.setScene(jadefx::make<jadefx::Scene>(root, kWidth, kHeight));
    const char* path = "jadefx-texture.ppm";
#if defined(_WIN32)
    _putenv_s("JADEFX_DUMP_PPM", path);
#else
    setenv("JADEFX_DUMP_PPM", path, 1);
#endif
    int pointWidth = 0;
    int pointHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    host.windowSize(pointWidth, pointHeight);
    host.framebufferSize(framebufferWidth, framebufferHeight);
    const bool first = stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
    const bool second = stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
#if defined(_WIN32)
    _putenv_s("JADEFX_DUMP_PPM", "");
#else
    unsetenv("JADEFX_DUMP_PPM");
#endif
    std::vector<unsigned char> pixels;
    FILE* file = std::fopen(path, "rb");
    int width = 0;
    int height = 0;
    if (file != nullptr && std::fscanf(file, "P6\n%d %d\n255\n", &width, &height) == 2 && width == kWidth &&
        height == kHeight) {
        pixels.resize(kFrameBytes);
        if (std::fread(pixels.data(), 1, pixels.size(), file) != pixels.size()) {
            pixels.clear();
        }
    }
    if (file != nullptr) {
        std::fclose(file);
    }
    std::remove(path);
    ok = first && second && stage.graphicsOk();
    stage.shutdownGraphics();
    host.destroy();
    return pixels;
}

// The RGB at x, y (top row first) of a frame.
const unsigned char* At(const std::vector<unsigned char>& pixels, int x, int y) {
    return pixels.data() + (static_cast<std::size_t>(y) * kWidth + x) * 3;
}

void TestPremultipliedFlipped() {
    auto node = jadefx::make<TextureNode>(static_cast<unsigned char>(255), 1.f, jadefx::TextureAlpha::Premultiplied);
    bool ok = false;
    const std::vector<unsigned char> pixels = RenderFrame(node, ok);
    Expect(ok, "TX1 drawing a texture is no GL error");
    if (pixels.size() != kFrameBytes) {
        Expect(false, "TX1 frame");
        return;
    }
    Expect(At(pixels, 120, 20)[0] > 200 && At(pixels, 120, 20)[2] < 50, "TX1 flipped, the texture's top row is on top");
    Expect(At(pixels, 120, 140)[2] > 200 && At(pixels, 120, 140)[0] < 50, "TX1 and its bottom row below");
}

void TestPremultipliedOpacity() {
    // Premultiplied red at alpha 128, drawn at opacity 0.5 over black: color is
    // scaled too, so red comes out near 64. Scaling alpha alone would leave it near 128.
    auto node = jadefx::make<TextureNode>(static_cast<unsigned char>(128), 0.5f, jadefx::TextureAlpha::Premultiplied);
    bool ok = false;
    const std::vector<unsigned char> pixels = RenderFrame(node, ok);
    if (pixels.size() != kFrameBytes) {
        Expect(false, "TX2 frame");
        return;
    }
    const int red = At(pixels, 120, 20)[0];
    Expect(red > 50 && red < 80, "TX2 opacity scales a premultiplied texture's color (red " + std::to_string(red) + ")");
}

void TestStraight() {
    // A straight draw is no GL error, and leaves the blend as the next UI draw expects.
    auto node = jadefx::make<TextureNode>(static_cast<unsigned char>(128), 1.f, jadefx::TextureAlpha::Straight);
    bool ok = false;
    const std::vector<unsigned char> pixels = RenderFrame(node, ok);
    Expect(ok && pixels.size() == kFrameBytes, "TX3 a straight texture draws");
}

}  // namespace

int RunTextureDrawTests() {
    TestPremultipliedFlipped();
    TestPremultipliedOpacity();
    TestStraight();
    if (gFailures == 0) {
        std::printf("texture draw tests passed\n");
    }
    return gFailures;
}
