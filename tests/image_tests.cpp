#include "jadefx/jadefx.hpp"

#include "platform/GlfwHost.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b, double epsilon = 0.75) { return std::fabs(a - b) <= epsilon; }

// 4x4 opaque red PNG.
constexpr unsigned char kRedPng[] = {
    137, 80,  78, 71,  13, 10, 26,  10,  0,  0,  0,   13,  73,  72,  68,  82,  0,   0,   0,  4,  0, 0,  0,   4,
    8,   6,   0,  0,   0,  169, 241, 158, 126, 0, 0,   0,   18,  73,  68,  65,  84,  120, 218, 99, 248, 207, 192,
    240, 31,  25, 51,  144, 46,  0,   0,   60,  64, 31,  225, 26, 243, 165, 72,  0,   0,   0,  0,  73, 69,  78,
    68,  174, 66, 96,  130};

void TestDecode() {
    const std::shared_ptr<jadefx::Image> image = jadefx::Image::load(kRedPng, sizeof kRedPng);
    Expect(image != nullptr && image->getWidth() == 4 && image->getHeight() == 4, "a png decodes to its pixel size");
    Expect(jadefx::Image::load(nullptr, 0) == nullptr, "empty bytes do not decode");
    const unsigned char garbage[] = {1, 2, 3, 4};
    Expect(jadefx::Image::load(garbage, sizeof garbage) == nullptr, "bytes that are not an image do not decode");
    Expect(jadefx::Image::load("") == nullptr, "an empty path does not decode");
    Expect(jadefx::Image::load("no-such-image.png") == nullptr, "a missing file does not decode");

    auto view = jadefx::make<jadefx::ImageView>(image);
    Expect(Near(view->measuredWidth(100), 4) && Near(view->measuredHeight(4, 100), 4),
           "an image view prefers the bitmap size");
    view->setPrefSize(16, 16);
    Expect(Near(view->getPrefWidth(), 16) && Near(view->getPrefHeight(), 16), "a preferred size can scale the view");
}

jadefx::Node* LabelNamed(jadefx::Node& root, const std::string& text) {
    for (jadefx::Node* node : root.getElementsByClassName("tree-cell-label")) {
        auto* label = dynamic_cast<jadefx::Label*>(node);
        if (label != nullptr && label->getText() == text) {
            return label;
        }
    }
    return nullptr;
}

void TestTreeIcon() {
    const std::shared_ptr<jadefx::Image> image = jadefx::Image::load(kRedPng, sizeof kRedPng);
    auto icon = jadefx::make<jadefx::ImageView>(image);
    auto root = jadefx::make<jadefx::TreeItem>("Root");
    root->setExpanded(true);
    auto script = jadefx::make<jadefx::TreeItem>("Script");
    script->setGraphic(icon);
    auto next = jadefx::make<jadefx::TreeItem>("Next");
    root->getChildren().add(script);
    root->getChildren().add(next);

    auto tree = jadefx::make<jadefx::TreeView>(root);
    tree->setShowRoot(false);
    tree->setFixedCellSize(24);
    tree->setPrefSize(220, 80);
    auto scene = jadefx::make<jadefx::Scene>(tree, 220, 80);
    scene->layout(220, 80, 0);

    Expect(Near(icon->getWidth(), 4) && Near(icon->getHeight(), 4), "the row keeps the icon's pixel size");
    jadefx::Node* label = LabelNamed(*scene, "Script");
    Expect(label != nullptr && icon->getAbsoluteX() + icon->getWidth() <= label->getAbsoluteX() + 0.5,
           "the icon sits to the left of the label");

    const double x = icon->getAbsoluteX() + icon->getWidth() * 0.5;
    const double y = icon->getAbsoluteY() + icon->getHeight() * 0.5;
    scene->noteButton(0, true, x, y);
    scene->noteButton(0, false, x, y);
    Expect(tree->getSelectedItem() == script.get(), "clicking the icon selects the row");
    scene->noteKey(jadefx::Key::Down, true, false, 0);
    Expect(tree->getSelectedItem() == next.get(), "arrow keys still move the selection after an icon click");
}

int CountRed(const std::vector<unsigned char>& pixels) {
    int red = 0;
    for (std::size_t i = 0; i + 2 < pixels.size(); i += 3) {
        if (pixels[i] > 200 && pixels[i + 1] < 80 && pixels[i + 2] < 80) {
            ++red;
        }
    }
    return red;
}

void TestImageFrame() {
    jadefx::GlfwHost host;
    if (!host.create(240, 160, "image")) {
        Expect(false, "image window opens");
        return;
    }
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        Expect(false, "image context");
        host.destroy();
        return;
    }

    const std::shared_ptr<jadefx::Image> image = jadefx::Image::load(kRedPng, sizeof kRedPng);
    auto view = jadefx::make<jadefx::ImageView>(image);
    view->setPrefSize(80, 80);
    stage.setScene(jadefx::make<jadefx::Scene>(view, 240, 160));

    const char* path = "jadefx-image.ppm";
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

    FILE* file = std::fopen(path, "rb");
    int width = 0;
    int height = 0;
    const bool header = file != nullptr && std::fscanf(file, "P6\n%d %d\n255\n", &width, &height) == 2 && width > 0 &&
                        height > 0;
    std::vector<unsigned char> pixels;
    if (header) {
        pixels.resize(static_cast<std::size_t>(width * height * 3));
        if (std::fread(pixels.data(), 1, pixels.size(), file) != pixels.size()) {
            pixels.clear();
        }
    }
    if (file != nullptr) {
        std::fclose(file);
    }
    std::remove(path);
    stage.shutdownGraphics();
    host.destroy();

    Expect(first && second && stage.graphicsOk() && !pixels.empty(), "an image frame draws without a GL error");
    Expect(CountRed(pixels) > 1000, "the bitmap is visible in the window");
}

}  // namespace

int RunImageTests() {
    TestDecode();
    TestTreeIcon();
    TestImageFrame();
    return gFailures;
}
