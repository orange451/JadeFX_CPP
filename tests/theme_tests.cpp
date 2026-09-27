#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>

// The cascade (specificity, !important, var()) and the built-in themes.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Same(const jadefx::Color& a, const jadefx::Color& b) { return jadefx::near(a, b); }

jadefx::Color Background(const jadefx::Node& node) { return node.computedStyle().background.color; }

struct Rig {
    std::shared_ptr<jadefx::Pane> root = jadefx::make<jadefx::Pane>();
    std::shared_ptr<jadefx::Scene> scene = jadefx::make<jadefx::Scene>(root, 300, 200);

    template <typename T>
    std::shared_ptr<T> add(std::shared_ptr<T> node) {
        root->getChildren().add(node);
        return node;
    }
    void frame() { scene->layout(300, 200, 0); }
};

void TestVariables() {
    Rig rig;
    auto box = rig.add(jadefx::make<jadefx::Pane>());
    box->getClassList().add("box");
    rig.scene->setStylesheet(
        ":root { --brand: #ff0000; --ink: var(--brand); }"
        ".box { background-color: var(--ink); color: var(--missing, #00ff00); border-color: var(--missing); }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(255, 0, 0)), "var() reads a custom property, through another var()");
    Expect(Same(box->computedStyle().color, jadefx::Color::rgb8(0, 255, 0)), "an unset var() takes its fallback");
    Expect(box->computedStyle().borderColor.a == 0, "an unset var() without a fallback drops the declaration");

    box->setStyle("--brand: #0000ff;");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(255, 0, 0)),
           "a custom property is resolved where it is used, from the value it has there");
    rig.scene->setStylesheet(":root { --brand: #ff0000; } .box { background-color: var(--brand); }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(0, 0, 255)), "a node's own custom property wins over :root's");
}

void TestSpecificityAndImportance() {
    Rig rig;
    auto box = rig.add(jadefx::make<jadefx::Pane>());
    box->getClassList().add("box");
    box->setElementId("only");
    rig.scene->setStylesheet(
        "#only { background-color: #ff0000; }"
        ".box { background-color: #00ff00; }"
        "pane { background-color: #0000ff; }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(255, 0, 0)), "an id beats a class and a type, whatever the order");
    rig.scene->setStylesheet(".box { background-color: #00ff00; } pane.box { background-color: #0000ff; }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(0, 0, 255)), "a type and a class beat a class alone");
    rig.scene->setStylesheet(".box { background-color: #00ff00; } .box { background-color: #0000ff; }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(0, 0, 255)), "at equal specificity the later rule wins");
    box->setStyle("background-color: #ffffff;");
    rig.scene->setStylesheet(".box { background-color: #00ff00 !important; }");
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(0, 255, 0)), "!important beats an inline style");
}

void TestOrigins() {
    Rig rig;
    auto plain = rig.add(jadefx::make<jadefx::Button>("Plain"));
    auto coded = rig.add(jadefx::make<jadefx::Button>("Coded"));
    coded->setBackground(jadefx::Color::rgb8(255, 0, 0));
    rig.frame();
    Expect(Same(Background(*plain), jadefx::Color::white()), "the light theme makes a button white");
    Expect(Same(Background(*coded), jadefx::Color::rgb8(255, 0, 0)), "a color set from code wins over the theme");
    rig.scene->setStylesheet("button { background-color: #00ff00; }");
    rig.frame();
    Expect(Same(Background(*coded), jadefx::Color::rgb8(0, 255, 0)), "an application stylesheet wins over code");
}

void TestThemes() {
    Rig rig;
    auto button = rig.add(jadefx::make<jadefx::Button>("Save"));
    auto box = rig.add(jadefx::make<jadefx::CheckBox>("Remember"));
    rig.frame();
    const jadefx::Color lightAccent = box->themeColor(jadefx::ThemeColor::Accent);
    Expect(Same(lightAccent, jadefx::Color::rgb8(26, 115, 232)), "the light accent is blue");
    Expect(Same(rig.scene->themeColor(jadefx::ThemeColor::Background), jadefx::Color::rgb8(248, 248, 248)),
           "the light scene background");

    rig.scene->setUserAgentStylesheet(jadefx::Theme::DARK);
    rig.frame();
    Expect(Same(Background(*button), jadefx::Color::rgb8(0x29, 0x2a, 0x2d)), "the dark theme darkens a button");
    Expect(Same(button->computedStyle().color, jadefx::Color::rgb8(0xe8, 0xea, 0xed)), "and lightens its text");
    Expect(Same(box->themeColor(jadefx::ThemeColor::Accent), jadefx::Color::rgb8(0x8a, 0xb4, 0xf8)),
           "controls draw with the dark accent");

    rig.scene->setStylesheet("checkbox { accent-color: #7b1fa2; }");
    rig.frame();
    Expect(Same(box->themeColor(jadefx::ThemeColor::Accent), jadefx::Color::rgb8(0x7b, 0x1f, 0xa2)),
           "accent-color sets one control's accent");
    Expect(Same(button->themeColor(jadefx::ThemeColor::Accent), jadefx::Color::rgb8(0x8a, 0xb4, 0xf8)),
           "and leaves the others");

    rig.scene->setUserAgentStylesheet("");
    jadefx::Application::setUserAgentStylesheet(jadefx::Application::STYLESHEET_DARK);
    rig.frame();
    Expect(Same(Background(*button), jadefx::Color::rgb8(0x29, 0x2a, 0x2d)), "the application's theme reaches every scene");
    rig.scene->setUserAgentStylesheet(jadefx::Theme::LIGHT);
    rig.frame();
    Expect(Same(Background(*button), jadefx::Color::white()), "a scene's own theme wins over the application's");
    jadefx::Application::setUserAgentStylesheet("");
    rig.scene->setUserAgentStylesheet("");
    rig.frame();
    Expect(Same(Background(*button), jadefx::Color::white()), "clearing both returns to light");
    Expect(jadefx::Theme::stylesheet("sepia").empty(), "only light and dark are built in");
}

}  // namespace

int RunThemeTests() {
    TestVariables();
    TestSpecificityAndImportance();
    TestOrigins();
    TestThemes();
    if (gFailures == 0) {
        std::printf("theme tests passed\n");
    }
    return gFailures;
}
