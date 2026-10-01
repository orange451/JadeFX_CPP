#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>

// SubScene: a part of a scene with a cascade of its own.
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

// An outer button beside a SubScene whose root holds an inner button.
struct Rig {
    std::shared_ptr<jadefx::StackPane> outer = jadefx::make<jadefx::StackPane>();
    std::shared_ptr<jadefx::Scene> scene = jadefx::make<jadefx::Scene>(outer, 300, 200);
    std::shared_ptr<jadefx::Button> outside = jadefx::make<jadefx::Button>("Outside");
    std::shared_ptr<jadefx::Pane> root = jadefx::make<jadefx::Pane>();
    std::shared_ptr<jadefx::SubScene> sub = jadefx::make<jadefx::SubScene>(root);
    std::shared_ptr<jadefx::Button> inside = jadefx::make<jadefx::Button>("Inside");

    Rig() {
        outer->getChildren().add(outside);
        outer->getChildren().add(sub);
        root->getChildren().add(inside);
    }
    void frame() { scene->layout(300, 200, 0); }
};

void TestUserAgentStylesheet() {
    Rig rig;
    rig.scene->setUserAgentStylesheet(jadefx::Theme::DARK);
    rig.frame();
    const jadefx::Color darkSurface = jadefx::Color::rgb8(0x29, 0x2a, 0x2d);
    Expect(Same(Background(*rig.outside), darkSurface), "the scene's theme styles the outer button");
    Expect(Same(Background(*rig.inside), darkSurface), "with no sheet of its own, a SubScene uses the scene's");

    rig.sub->setUserAgentStylesheet("button { background-color: #ff0000; }");
    rig.frame();
    Expect(Same(Background(*rig.inside), jadefx::Color::rgb8(255, 0, 0)), "a SubScene's own sheet styles what is inside");
    Expect(Same(Background(*rig.outside), darkSurface), "and leaves the outer scene alone");

    rig.sub->setUserAgentStylesheet("label { color: #00ff00; }");
    rig.frame();
    Expect(Background(*rig.inside).a == 0.f, "the scene's theme rules do not reach inside a SubScene with its own sheet");
    Expect(Same(rig.inside->themeColor(jadefx::ThemeColor::Border), jadefx::Theme::defaultColor(jadefx::ThemeColor::Border)),
           "nor do its custom properties");

    rig.sub->setUserAgentStylesheet(jadefx::Theme::LIGHT);
    rig.frame();
    Expect(Same(Background(*rig.inside), jadefx::Color::white()), "a SubScene takes light or dark as a scene does");
    Expect(rig.sub->getUserAgentStylesheet() == jadefx::Theme::LIGHT, "and reports what it was given");
}

void TestAuthorStylesheetsStop() {
    Rig rig;
    rig.scene->setStylesheet("button { color: #ff0000; } .mark { color: #ff0000; }");
    rig.outer->setStylesheet("button { border-width: 3px; border-style: solid; }");
    rig.sub->setStylesheet("button { color: #0000ff; }");
    rig.sub->setUserAgentStylesheet("button { color: #00ff00; }");
    rig.inside->getClassList().add("mark");
    rig.frame();
    Expect(Same(rig.outside->computedStyle().color, jadefx::Color::rgb8(255, 0, 0)), "the scene's stylesheet styles the outer button");
    Expect(Same(rig.inside->computedStyle().color, jadefx::Color::rgb8(0, 255, 0)),
           "stylesheets on the scene and on the SubScene do not reach inside");
    Expect(rig.inside->computedStyle().border.top == 0, "nor do stylesheets on the SubScene's ancestors");

    rig.root->setStylesheet("button { color: #00ffff; }");
    rig.frame();
    Expect(Same(rig.inside->computedStyle().color, jadefx::Color::rgb8(0, 255, 255)), "the root's own stylesheet applies inside");
}

void TestNothingInherits() {
    Rig rig;
    rig.scene->setStylesheet(":root { --brand: #ff0000; color: #ff0000; font-size: 30px; } stackpane { --edge: #ff0000; }");
    rig.sub->setUserAgentStylesheet("label { color: #00ff00; }");
    auto box = jadefx::make<jadefx::Pane>();
    box->setStyle("background-color: var(--brand, #00ff00); border-color: var(--edge, #0000ff);");
    rig.root->getChildren().add(box);
    rig.frame();
    Expect(Same(Background(*box), jadefx::Color::rgb8(0, 255, 0)), "a custom property on :root outside does not reach inside");
    Expect(Same(box->computedStyle().borderColor, jadefx::Color::rgb8(0, 0, 255)), "nor one on the SubScene's parent");
    Expect(Same(rig.inside->computedStyle().color, jadefx::Color::black()), "text color does not inherit across the boundary");
    Expect(rig.inside->computedStyle().fontSize == 16.f, "nor does the font size");
    Expect(Same(rig.outside->computedStyle().color, jadefx::Color::rgb8(255, 0, 0)), "while outside it still does");
}

void TestRoot() {
    Rig rig;
    rig.sub->setUserAgentStylesheet(":root { --brand: #0000ff; } button { background-color: var(--brand); }");
    rig.frame();
    Expect(rig.root->pseudoState("root"), "the SubScene's root matches :root");
    Expect(Same(Background(*rig.inside), jadefx::Color::rgb8(0, 0, 255)), "so :root variables reach the nodes inside");

    auto next = jadefx::make<jadefx::Pane>();
    rig.sub->setRoot(next);
    Expect(!rig.root->pseudoState("root") && next->pseudoState("root"), "setRoot moves :root to the new root");
    Expect(rig.sub->getRoot() == next.get() && rig.root->getParent() == nullptr, "and lets the old root go");

    rig.outer->getChildren().add(next);
    Expect(rig.sub->getRoot() == nullptr && !next->pseudoState("root"), "a root moved elsewhere is no longer the root");
}

void TestOuterStylesTheSubScene() {
    Rig rig;
    rig.scene->setStylesheet("subscene { background-color: #ff0000; }");
    rig.sub->setUserAgentStylesheet("subscene { background-color: #00ff00; }");
    rig.frame();
    Expect(Same(Background(*rig.sub), jadefx::Color::rgb8(255, 0, 0)), "the outer cascade styles the SubScene node itself");
}

void TestApplyCssMatchesFullPass() {
    Rig rig;
    rig.scene->setStylesheet(":root { --brand: #ff0000; color: #ff0000; } button { border-width: 3px; border-style: solid; }");
    rig.sub->setUserAgentStylesheet("label { color: #00ff00; }");
    rig.inside->setStyle("background-color: var(--brand, #00ff00);");
    rig.frame();
    rig.inside->applyCss();
    Expect(Same(rig.inside->computedStyle().color, jadefx::Color::black()), "applyCss on an inner node keeps the boundary for inheritance");
    Expect(Same(Background(*rig.inside), jadefx::Color::rgb8(0, 255, 0)), "and for custom properties");
    Expect(rig.inside->computedStyle().border.top == 0, "and for stylesheets");
    rig.root->applyCss();
    Expect(Same(rig.root->computedStyle().color, jadefx::Color::black()), "applyCss on the root starts it fresh");
}

void TestLayout() {
    Rig rig;
    rig.root->setPrefSize(50, 40);
    rig.frame();
    Expect(rig.sub->getWidth() == 50 && rig.sub->getHeight() == 40, "a SubScene measures as its root");
    Expect(rig.root->getWidth() == 50 && rig.root->getHeight() == 40 && rig.root->getX() == 0 && rig.root->getY() == 0,
           "and lays its root out over its content box");

    rig.sub->setPrefSize(120, 80);
    rig.sub->setPadding(jadefx::Insets::uniform(5));
    rig.frame();
    Expect(rig.root->getWidth() == 110 && rig.root->getHeight() == 70 && rig.root->getX() == 5 && rig.root->getY() == 5,
           "the root fills the SubScene's content box whatever it would rather be");
}

void TestPickStaysInside() {
    Rig rig;
    rig.sub->setPrefSize(100, 100);
    rig.sub->setPickOnBounds(false);
    rig.root->setPickOnBounds(false);
    rig.inside->setPrefSize(20, 20);
    rig.frame();
    const double x = rig.inside->getAbsoluteX() + 5;
    const double y = rig.inside->getAbsoluteY() + 5;
    Expect(rig.scene->pick(x, y) == rig.inside.get(), "a node inside a SubScene is picked as any node");
    rig.inside->setTranslateX(200);
    rig.frame();
    Expect(rig.scene->pick(rig.inside->getAbsoluteX() + 5, rig.inside->getAbsoluteY() + 5) != rig.inside.get(),
           "but not outside the SubScene's bounds, where it is clipped");
}

}  // namespace

int RunSubSceneTests() {
    TestUserAgentStylesheet();
    TestAuthorStylesheetsStop();
    TestNothingInherits();
    TestRoot();
    TestOuterStylesTheSubScene();
    TestApplyCssMatchesFullPass();
    TestLayout();
    TestPickStaysInside();
    if (gFailures == 0) {
        std::printf("subscene tests passed\n");
    }
    return gFailures;
}
