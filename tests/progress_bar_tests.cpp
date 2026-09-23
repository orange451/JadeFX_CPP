#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <limits>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b) {
    const double delta = a - b;
    return delta < 0.02 && delta > -0.02;
}

bool SameColor(const jadefx::Color& have, const jadefx::Color& want) {
    return Near(have.r, want.r) && Near(have.g, want.g) && Near(have.b, want.b) && Near(have.a, want.a);
}

void TestDefaults() {
    auto bar = jadefx::make<jadefx::ProgressBar>();
    Expect(std::string(bar->getElementType()) == "progress-bar", "the element type is progress-bar");
    Expect(bar->getProgress() == jadefx::ProgressBar::INDETERMINATE_PROGRESS, "a progress bar starts indeterminate");
    Expect(bar->isIndeterminate(), "the initial progress is indeterminate");
    Expect(bar->pseudoState("indeterminate") && !bar->pseudoState("determinate"), "the indeterminate pseudo starts on");
    Expect(bar->getIndeterminateBarLength() == 60, "the traveling bar is 60px");
    Expect(bar->isIndeterminateBarEscape() && bar->isIndeterminateBarFlip(), "the traveling bar escapes and flips");
    Expect(bar->getIndeterminateBarAnimationTime() == 2, "each pass takes 2 seconds");
    Expect(bar->getTrack() != nullptr && std::string(bar->getTrack()->getElementType()) == "track", "the groove type is track");
    Expect(bar->getBar() != nullptr && std::string(bar->getBar()->getElementType()) == "bar", "the fill type is bar");
}

void TestProgressValues() {
    auto bar = jadefx::make<jadefx::ProgressBar>(0.25);
    Expect(!bar->isIndeterminate() && bar->pseudoState("determinate"), "0.25 is determinate");
    Expect(!bar->pseudoState("indeterminate"), "determinate clears the indeterminate pseudo");
    Expect(bar->getProgress() == 0.25, "getProgress returns 0.25");

    bar->setProgress(1.5);
    Expect(!bar->isIndeterminate() && bar->getProgress() == 1.5, "a value above 1 stays determinate");

    bar->setProgress(0);
    Expect(!bar->isIndeterminate() && bar->pseudoState("determinate"), "zero is an empty determinate bar");

    bar->setProgress(-0.2);
    Expect(bar->isIndeterminate() && bar->pseudoState("indeterminate"), "any negative progress is indeterminate");
    Expect(bar->getProgress() == -0.2, "a negative progress is kept");

    bar->setProgress(std::numeric_limits<double>::quiet_NaN());
    Expect(!bar->isIndeterminate() && bar->pseudoState("determinate"), "NaN progress is determinate");
}

void TestDeterminateBar() {
    auto bar = jadefx::make<jadefx::ProgressBar>(0.5);
    bar->setPrefWidth(200);
    auto scene = jadefx::make<jadefx::Scene>(bar, 400, 80);
    scene->layout(400, 80, 0);

    Expect(Near(bar->getWidth(), 200), "pref width is the laid out width");
    Expect(Near(bar->getHeight(), 24), "height is 1.5em at the 16px font");
    Expect(Near(bar->getTrack()->getWidth(), 200) && Near(bar->getTrack()->getHeight(), 24), "the track fills the bar");
    Expect(Near(bar->getBar()->getWidth(), 100), "half progress fills half the track");
    Expect(Near(bar->getBar()->getHeight(), 24), "the fill is as tall as the track");
    Expect(Near(bar->getBar()->getTranslateX(), 0), "a determinate fill is not translated");
    Expect(SameColor(bar->getBar()->computedStyle().background.color, jadefx::Color::parse("#1a73e8")),
           "the determinate fill uses the accent");
    Expect(!bar->getBar()->computedStyle().background.gradient, "the determinate fill is a solid color");
    Expect(SameColor(bar->getTrack()->computedStyle().background.color, jadefx::Color::parse("#f1f3f4")),
           "the track is a light groove");
    Expect(Near(bar->getTrack()->computedStyle().border.left, 1), "the track keeps a 1px border");
    Expect(SameColor(bar->getTrack()->computedStyle().borderColor, jadefx::Color::parse("#dadce0")),
           "the track border matches the other controls");
    const double radius = jadefx::resolveSize(bar->getTrack()->computedStyle().radius[0], 24, 16);
    Expect(Near(radius, 4), "the track corner radius is 4");

    bar->setProgress(1.5);
    scene->layout(400, 80, 0);
    Expect(Near(bar->getBar()->getWidth(), 200), "progress above 1 fills the track");

    bar->setProgress(0);
    scene->layout(400, 80, 0);
    Expect(Near(bar->getBar()->getWidth(), 0), "zero progress draws an empty fill");

    bar->setProgress(std::numeric_limits<double>::quiet_NaN());
    scene->layout(400, 80, 0);
    Expect(Near(bar->getBar()->getWidth(), 0), "NaN progress draws an empty fill");

    bar->setPrefWidth(10);
    bar->setProgress(0.26);
    scene->layout(400, 80, 0);
    Expect(Near(bar->getBar()->getWidth(), 2.5), "fill width truncates to a half pixel");
}

void TestPreferredSizeDoesNotStretch() {
    auto bar = jadefx::make<jadefx::ProgressBar>(0.5);
    auto pane = jadefx::make<jadefx::BorderPane>();
    pane->setPrefSize(400, 120);
    pane->setCenter(bar);
    auto scene = jadefx::make<jadefx::Scene>(pane, 400, 120);
    scene->layout(400, 120, 0);
    Expect(Near(pane->getWidth(), 400), "the pane uses its preferred size");
    Expect(Near(bar->getWidth(), 100), "the bar stays at its 100px preferred width");
    Expect(Near(bar->getHeight(), 24), "the bar stays at its preferred height");
    Expect(Near(bar->getBar()->getWidth(), 50), "half of the preferred width is filled");

    bar->setMaxSize(400, 80);
    scene->layout(400, 120, 0);
    Expect(Near(bar->getWidth(), 400), "a larger maximum lets the bar use the slot");
    Expect(Near(bar->getHeight(), 80), "the height stops at the maximum");
    Expect(Near(bar->getBar()->getWidth(), 200), "the fill follows the grown width");
}

void TestIndeterminateTravel() {
    auto bar = jadefx::make<jadefx::ProgressBar>();
    auto scene = jadefx::make<jadefx::Scene>(bar, 200, 80);

    scene->layout(200, 80, 0);
    Expect(Near(bar->getWidth(), 100), "an indeterminate bar still prefers 100px");
    Expect(Near(bar->getBar()->getWidth(), 60), "the traveling bar uses its length");
    Expect(Near(bar->getBar()->getTranslateX(), -60), "the pass starts off the left end");
    Expect(bar->getBar()->computedStyle().background.gradient, "the traveling fill is a gradient");
    Expect(Near(bar->getBar()->computedStyle().background.angleDeg, 90), "the bright end leads on the way forward");

    scene->layout(200, 80, 0.8);
    Expect(Near(bar->getBar()->getTranslateX(), -20), "ease-both is partway along at 0.8s");

    scene->layout(200, 80, 1);
    Expect(Near(bar->getBar()->getTranslateX(), 0), "the fill reaches the left edge at 1s");

    scene->layout(200, 80, 2);
    Expect(Near(bar->getBar()->getTranslateX(), 100), "the fill reaches the right end at 2s");
    Expect(Near(bar->getBar()->computedStyle().background.angleDeg, 90), "the forward pass stays mirrored through the midpoint");

    scene->layout(200, 80, 3);
    Expect(Near(bar->getBar()->computedStyle().background.angleDeg, 270), "the return pass puts the bright end on the left");

    scene->layout(200, 80, 4);
    Expect(Near(bar->getBar()->getTranslateX(), -60), "the cycle starts again at 4s");

    bar->setProgress(0.25);
    scene->layout(200, 80, 4);
    Expect(!bar->isIndeterminate(), "setting progress leaves the indeterminate state");
    Expect(Near(bar->getBar()->getTranslateX(), 0), "a determinate fill clears the travel offset");
    Expect(Near(bar->getBar()->getWidth(), 25), "the fill is a quarter of the 100px track");
    Expect(!bar->getBar()->computedStyle().background.gradient, "the fill is solid again");
}

void TestTravelFlags() {
    auto bar = jadefx::make<jadefx::ProgressBar>();
    bar->setIndeterminateBarEscape(false);
    auto scene = jadefx::make<jadefx::Scene>(bar, 200, 80);
    scene->layout(200, 80, 0);
    Expect(!bar->isIndeterminateBarEscape(), "escape can be turned off");
    Expect(Near(bar->getBar()->getTranslateX(), 0), "without escape the fill starts inside the track");
    scene->layout(200, 80, 2);
    Expect(Near(bar->getBar()->getTranslateX(), 40), "without escape the fill stops at the right padding");

    bar->setIndeterminateBarEscape(true);
    bar->setIndeterminateBarFlip(false);
    scene->layout(200, 80, 0);
    Expect(bar->isIndeterminateBarEscape() && !bar->isIndeterminateBarFlip(), "flip and escape are independent");
    scene->layout(200, 80, 1);
    Expect(Near(bar->getBar()->getTranslateX(), 20), "without flip the fill is halfway at 1s");
}

void TestStyles() {
    auto bar = jadefx::make<jadefx::ProgressBar>();
    auto scene = jadefx::make<jadefx::Scene>(bar, 240, 80);
    scene->setStylesheet(
        "progress-bar:indeterminate { background-color: #111111; }"
        "progress-bar:determinate { background-color: #222222; }"
        "progress-bar {"
        "  indeterminate-bar-length: 30px;"
        "  indeterminate-bar-escape: false;"
        "  indeterminate-bar-flip: false;"
        "  indeterminate-bar-animation-time: 1;"
        "}"
        "progress-bar > track { background-color: #112233; border-width: 0; }"
        "progress-bar > bar { background-color: #00aa00; padding: 4px; border-radius: 0; }");
    scene->layout(240, 80, 0);

    Expect(SameColor(bar->computedStyle().background.color, jadefx::Color::parse("#111111")),
           ":indeterminate matches an indeterminate progress bar");
    Expect(Near(bar->getIndeterminateBarLength(), 30), "CSS sets the traveling length");
    Expect(!bar->isIndeterminateBarEscape() && !bar->isIndeterminateBarFlip(), "CSS turns escape and flip off");
    Expect(Near(bar->getIndeterminateBarAnimationTime(), 1), "CSS sets the pass duration");
    Expect(Near(bar->getHeight(), 8), "bar padding sets the progress bar height");
    Expect(Near(bar->getBar()->getWidth(), 30), "the traveling bar uses the CSS length");
    Expect(Near(bar->getBar()->getTranslateX(), 0), "CSS escape false starts the fill inside the track");
    Expect(SameColor(bar->getTrack()->computedStyle().background.color, jadefx::Color::parse("#112233")),
           "track background comes from CSS");
    Expect(Near(bar->getTrack()->computedStyle().border.left, 0), "a track border width of 0 removes the border");
    Expect(SameColor(bar->getBar()->computedStyle().background.color, jadefx::Color::parse("#00aa00")),
           "a styled fill keeps its color while traveling");
    Expect(!bar->getBar()->computedStyle().background.gradient, "a styled fill does not gain the default gradient");
    const double radius = jadefx::resolveSize(bar->getBar()->computedStyle().radius[0], 8, 16);
    Expect(Near(radius, 0), "a bar radius of 0 is kept");

    scene->layout(240, 80, 0.5);
    Expect(Near(bar->getBar()->getTranslateX(), 35), "a 1 second pass is halfway at 0.5s");

    bar->setProgress(0.5);
    scene->layout(240, 80, 0.5);
    Expect(SameColor(bar->computedStyle().background.color, jadefx::Color::parse("#222222")),
           ":determinate matches after progress is set");
    Expect(Near(bar->getBar()->getWidth(), 50) && Near(bar->getBar()->getTranslateX(), 0),
           "determinate layout replaces the traveling bar");
}

}  // namespace

int RunProgressBarTests() {
    TestDefaults();
    TestProgressValues();
    TestDeterminateBar();
    TestPreferredSizeDoesNotStretch();
    TestIndeterminateTravel();
    TestTravelFlags();
    TestStyles();
    return gFailures;
}
