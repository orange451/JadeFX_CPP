#pragma once

#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/paint/Color.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace jadefx {

// What font-smoothing: auto means on this platform. macOS, iOS, and Android draw
// text in grayscale, as their browsers do; elsewhere text uses LCD stripes.
#if defined(__APPLE__) || defined(__ANDROID__)
inline constexpr bool kSubpixelByDefault = false;
#else
inline constexpr bool kSubpixelByDefault = true;
#endif

enum class BorderStyle { None, Solid };

// Mouse cursor. Inherit and Auto are specified values; a resolved style uses a shape.
// hand is pointer. Resize edges share ew/ns/nwse/nesw. grab and grabbing are their own
// values and draw as the hand. wait, help, progress, zoom, alias, copy, and context-menu
// draw as the arrow, because the platform cursors do not include those glyphs.
enum class Cursor {
    Inherit,
    Auto,
    Default,
    Pointer,
    Text,
    Crosshair,
    Move,
    NotAllowed,
    EwResize,
    NsResize,
    NwseResize,
    NeswResize,
    None,
    Wait,
    Progress,
    Help,
    Grab,
    Grabbing,
    ZoomIn,
    ZoomOut,
    ContextMenu,
    Alias,
    Copy,
};

// A platform cursor. Several Cursor values share one shape.
enum class CursorShape {
    Arrow,
    IBeam,
    Crosshair,
    Hand,
    SizeWestEast,
    SizeNorthSouth,
    SizeNorthwestSoutheast,
    SizeNortheastSouthwest,
    SizeAll,
    NotAllowed,
    // The arrow with a plus: a drop here adds a copy.
    Copy,
    Hidden,
};

inline CursorShape cursorShape(Cursor cursor) {
    switch (cursor) {
        case Cursor::Text:
            return CursorShape::IBeam;
        case Cursor::Crosshair:
            return CursorShape::Crosshair;
        case Cursor::Pointer:
        case Cursor::Grab:
        case Cursor::Grabbing:
            return CursorShape::Hand;
        case Cursor::EwResize:
            return CursorShape::SizeWestEast;
        case Cursor::NsResize:
            return CursorShape::SizeNorthSouth;
        case Cursor::NwseResize:
            return CursorShape::SizeNorthwestSoutheast;
        case Cursor::NeswResize:
            return CursorShape::SizeNortheastSouthwest;
        case Cursor::Move:
            return CursorShape::SizeAll;
        case Cursor::NotAllowed:
            return CursorShape::NotAllowed;
        case Cursor::Copy:
            return CursorShape::Copy;
        case Cursor::None:
            return CursorShape::Hidden;
        case Cursor::Inherit:
        case Cursor::Auto:
        case Cursor::Default:
        case Cursor::Wait:
        case Cursor::Progress:
        case Cursor::Help:
        case Cursor::ZoomIn:
        case Cursor::ZoomOut:
        case Cursor::ContextMenu:
        case Cursor::Alias:
            return CursorShape::Arrow;
    }
    return CursorShape::Arrow;
}

// True when text is a cursor keyword. The first supported keyword in a comma list wins.
bool parseCursor(std::string_view text, Cursor& cursor);

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

// CSS custom properties (--name: value), which every node inherits from its
// parent. Shared between nodes until one declares its own.
using CssVariables = std::unordered_map<std::string, std::string>;

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
    bool subpixel = kSubpixelByDefault;
    SizeSpec radius[4] = {};
    Insets padding;
    Insets border;
    BorderStyle borderStyle = BorderStyle::None;
    std::vector<BoxShadow> shadows;
    float spacing = 0.f;
    // row-gap and column-gap, for panes that lay children in a grid or in runs.
    // Negative when no stylesheet sets them.
    float rowGap = -1.f;
    float columnGap = -1.f;
    Pos alignment = Pos::Ancestor;
    bool alignmentFromCss = false;
    float opacity = 1.f;
    // image-color. An image-view fills its bitmap's alpha with this color instead
    // of drawing the bitmap's own colors, so one white icon can take any color.
    // currentColor is the node's color, which a graphic inherits from its label,
    // so the icon follows the text. none, the default, draws the bitmap as it is.
    bool imageColorSet = false;
    bool imageColorCurrent = false;
    Color imageColor;
    // Resolved cursor for this node. Inherit and Auto do not appear after styling.
    Cursor cursor = Cursor::Default;
    SizeSpec width;
    SizeSpec height;
    SizeSpec minWidth;
    SizeSpec minHeight;
    SizeSpec maxWidth;
    SizeSpec maxHeight;
    // Set when a rule gives orientation.
    bool orientationFromCss = false;
    Orientation orientation = Orientation::Horizontal;
    // ProgressBar skin properties. Unset fields leave the control's own values.
    bool indeterminateBarLengthSet = false;
    SizeSpec indeterminateBarLength;
    bool indeterminateBarEscapeSet = false;
    bool indeterminateBarEscape = true;
    bool indeterminateBarFlipSet = false;
    bool indeterminateBarFlip = true;
    bool indeterminateBarAnimationTimeSet = false;
    double indeterminateBarAnimationTime = 2;
    // Property name, or "all", to duration and delay.
    std::unordered_map<std::string, TransitionTiming> transitions;
    // Custom properties in effect here, including the ones inherited.
    std::shared_ptr<const CssVariables> variables;

    // A custom property's value with any var() in it resolved. Empty when unset.
    std::string variable(std::string_view name) const;
};

// Each property applyDeclarations understands. Custom is a --name property, or
// accent-color, caret-color, or outline-color, which are stored as one. All names
// every property, for transition.
enum class PropertyId : unsigned char {
    Unknown,
    Custom,
    FontSize,
    FontFamily,
    BackgroundColor,
    Background,
    BackgroundImage,
    FontSmoothing,
    Color,
    ImageColor,
    Width,
    Height,
    MinWidth,
    MinHeight,
    MaxWidth,
    MaxHeight,
    BorderRadius,
    BorderWidth,
    BorderColor,
    BorderStyle,
    BoxShadow,
    Padding,
    Spacing,
    Gap,
    RowGap,
    ColumnGap,
    Alignment,
    Orientation,
    Opacity,
    IndeterminateBarLength,
    IndeterminateBarEscape,
    IndeterminateBarFlip,
    IndeterminateBarAnimationTime,
    Transition,
    Cursor,
    All,
    Count,
};

inline constexpr std::size_t kPropertyIdCount = static_cast<std::size_t>(PropertyId::Count);

// The id for a lower-case property name, or Custom, or Unknown.
PropertyId propertyIdOf(std::string_view property);

// A declaration's value, parsed when its stylesheet loads. Raw means it is parsed
// as each node applies it, as a value with var() must be. Invalid means it did not
// parse, so applying it changes nothing. Lengths keeps up to four lengths with px
// in pixels, % in percent, and em in em, as padding, border-width, and
// border-radius list them.
struct DeclarationValue {
    enum class Kind : unsigned char { Raw, Color, Size, Lengths, Invalid };
    Kind kind = Kind::Raw;
    Color color;
    SizeSpec size;
    int lengthCount = 0;
    SizeSpec lengths[4];
};

struct Declaration {
    std::string property;
    std::string value;
    // Declared with !important, which wins over normal declarations of any origin.
    bool important = false;
    PropertyId id = PropertyId::Unknown;
    // The value has var() in it and is resolved against each node's custom properties.
    bool hasVar = false;
    DeclarationValue parsed;
};

// A declaration a stylesheet matched, with the specificity of the selector that
// matched it: ids, then classes and pseudo-classes, then types.
struct MatchedDeclaration {
    const Declaration* declaration = nullptr;
    int specificity = 0;
};

// Replaces each var(--name) or var(--name, fallback) in a value. An unset name
// with no fallback leaves the value unusable, as in CSS, and returns empty.
std::string resolveCssVariables(std::string_view value, const CssVariables* variables);

class Node;

enum class StylePass { Variables, Fonts, Rest };

// Custom properties come first so every later value can use them. Fonts are next
// so `em` on later properties sees the cascaded font size. accent-color,
// caret-color, and outline-color are stored as the custom properties
// --accent-color, --caret-color, and --outline-color, which controls read.
// `inheritedFontSize` resolves `font-size` in em. `emFontSize` resolves every other em.
void applyDeclarations(ComputedStyle& style, const std::vector<const Declaration*>& declarations, StylePass pass,
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
    // Adds each declaration whose rule matches node, in source order.
    void collectMatching(Node& node, std::vector<MatchedDeclaration>& out) const;
    bool empty() const;

private:
    struct Data;
    // Rules are immutable after parse, so copies share them.
    std::shared_ptr<Data> data_;
};

}  // namespace jadefx
