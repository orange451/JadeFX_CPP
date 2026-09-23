#pragma once

#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/paint/Color.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace jadefx {

enum class BorderStyle { None, Solid };

enum class SizeKind { Unset, Pixels, Percent, Calc };

// A CSS length. Calc is `percent * available + pixels` (pixels may be negative).
struct SizeSpec {
    SizeKind kind = SizeKind::Unset;
    double pixels = 0;
    double percent = 0;
    // Added when the length is resolved. `1em` is one multiple of the element's font size.
    double em = 0;

    static SizeSpec unset() { return {}; }
    static SizeSpec px(double value) { return {SizeKind::Pixels, value, 0, 0}; }
    static SizeSpec ratio(double value) { return {SizeKind::Percent, 0, value, 0}; }
    static SizeSpec calc(double percent, double pixels) { return {SizeKind::Calc, pixels, percent, 0}; }

    bool set() const { return kind != SizeKind::Unset; }
};

inline double resolveSize(const SizeSpec& spec, double available, double fontSize = 0) {
    const double em = spec.em * fontSize;
    switch (spec.kind) {
        case SizeKind::Pixels:
            return spec.pixels + em;
        case SizeKind::Percent:
            return available * spec.percent + em;
        case SizeKind::Calc:
            return available * spec.percent + spec.pixels + em;
        case SizeKind::Unset:
            return em;
    }
    return em;
}

struct BoxShadow {
    double offsetX = 0;
    double offsetY = 0;
    double blur = 0;
    double spread = 0;
    Color color = Color::rgba(0.f, 0.f, 0.f, 0.25f);
    bool inset = false;
};

inline constexpr int kMaxGradientStops = 8;

struct Background {
    Color color = Color::transparent();
    bool hasColor = false;
    Color stops[kMaxGradientStops] = {};
    float stopAt[kMaxGradientStops] = {};
    int stopCount = 0;
    float angleDeg = 180.f;
    bool gradient = false;
    bool visible = false;
};

struct TransitionTiming {
    double duration = 0;
    double delay = 0;
};

// The resolved look of one node for this frame, after stylesheets and inline CSS.
struct ComputedStyle {
    Background background;
    Color color = Color::black();
    Color borderColor = Color::transparent();
    float fontSize = 16.f;
    std::string fontFamily = "Open Sans";
    // font-smoothing. True is subpixel-antialiased; false is grayscale.
    bool subpixel = true;
    SizeSpec radius[4] = {};
    Insets padding;
    Insets border;
    BorderStyle borderStyle = BorderStyle::None;
    std::vector<BoxShadow> shadows;
    float spacing = 0.f;
    Pos alignment = Pos::Ancestor;
    bool alignmentFromCss = false;
    float opacity = 1.f;
    SizeSpec width;
    SizeSpec height;
    SizeSpec minWidth;
    SizeSpec minHeight;
    SizeSpec maxWidth;
    SizeSpec maxHeight;
    // Property name, or "all", to duration and delay.
    std::unordered_map<std::string, TransitionTiming> transitions;
};

struct Declaration {
    std::string property;
    std::string value;
};

class Node;

enum class StylePass { Fonts, Rest };

// Fonts are applied first so `em` on later properties sees the cascaded font size.
// `inheritedFontSize` resolves `font-size` in em. `emFontSize` resolves every other em.
void applyDeclarations(ComputedStyle& style, const std::vector<Declaration>& declarations, StylePass pass,
                       float inheritedFontSize, float emFontSize);
std::vector<Declaration> parseInlineDeclarations(const std::string& css);

// A parsed CSS stylesheet. Copying shares the parsed rules.
class Stylesheet {
public:
    Stylesheet();
    Stylesheet(const Stylesheet& other);
    Stylesheet& operator=(const Stylesheet& other);
    Stylesheet(Stylesheet&& other) noexcept;
    Stylesheet& operator=(Stylesheet&& other) noexcept;
    ~Stylesheet();

    static Stylesheet parse(const std::string& css);
    void collectMatching(Node& node, std::vector<Declaration>& out) const;
    bool empty() const;

private:
    struct Data;
    // Rules are immutable after parse, so copies share them.
    std::shared_ptr<Data> data_;
};

}  // namespace jadefx
