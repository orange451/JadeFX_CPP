#pragma once

#include "jadefx/paint/Color.hpp"
#include "jadefx/style/Style.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace jadefx {

// A color the built-in look draws with. Each is a CSS custom property of the
// name variableName gives, such as --accent-color, so a stylesheet can change
// it for the whole scene (:root { --accent-color: #7b1fa2; }) or for one part.
enum class ThemeColor {
    Background,     // --background-color: the scene behind everything
    Surface,        // --surface-color: controls, lists, popups, and panels
    Text,           // --text-color
    Muted,          // --muted-color: secondary text, arrows, and unchecked boxes
    Faint,          // --faint-color: minor tick marks and hints
    Border,         // --border-color: control outlines and separators
    Accent,         // --accent-color: checked boxes, fills, and selections
    Outline,        // --outline-color: the focus ring
    Link,           // --link-color: links in rich text, as the LinkText system color
    Hover,          // --hover-color: a pressed box, a hovered menu title
    Track,          // --track-color: grooves, tab strips, and spinner buttons
    Subtle,         // --subtle-color: table headers and a hovered check box
    Selection,      // --selection-color: selected rows and hovered menu items
    SelectionHover, // --selection-hover-color: a selected row under the pointer
    RowHover,       // --row-hover-color: a row under the pointer
    Tab,            // --tab-color: a tab that is not selected
    Tooltip,        // --tooltip-color
    TooltipText,    // --tooltip-text-color
    Dimmer,         // --dimmer-color: the veil behind a dialog
    Wash,           // --wash-color: a hovered button's overlay, twice as strong when pressed
    TextSelection,  // --text-selection-color: selected text
    CurrentLine,    // --current-line-color: the caret's line in a code area
    Gutter,         // --gutter-color: a code area's line-number gutter
    Scrollbar,      // --scrollbar-color: the scroll bar thumb
    Divider,        // --divider-color: a split pane's dividers
    Grip,           // --grip-color: the grip on a divider
    Info,           // --info-color
    Warning,        // --warning-color
    Error,          // --error-color
    Success,        // --success-color
};

// How many ThemeColor values there are. Success is last.
inline constexpr std::size_t kThemeColorCount = static_cast<std::size_t>(ThemeColor::Success) + 1;

namespace detail {
// Forgets the theme colors parsed from custom properties. A theme change calls this.
void clearThemeColorCache();
}  // namespace detail

// The built-in looks, in the shape of JavaFX's user-agent stylesheets (Modena).
// A user-agent stylesheet is the lowest layer of the cascade: application
// stylesheets, inline styles, and colors set from code all win over it.
// light and dark share every rule and differ only in their ThemeColor values.
class Theme {
public:
    // Counts setUserAgentStylesheet calls, so a scene can see that the theme changed.
    static std::uint64_t generation();
    // Keywords for setUserAgentStylesheet, here and on Application and Scene.
    static constexpr const char* LIGHT = "light";
    static constexpr const char* DARK = "dark";

    static const char* variableName(ThemeColor color);
    // The light value, used when no stylesheet sets the property.
    static Color defaultColor(ThemeColor color);
    // The CSS of a built-in theme: its custom properties on :root and the rules
    // every theme shares. Empty for a name that is not light or dark.
    static std::string stylesheet(std::string_view name);

    // light, dark, or CSS text. Empty restores light. Every scene without its own uses it.
    static void setUserAgentStylesheet(std::string cssOrTheme);
    static const std::string& getUserAgentStylesheet();
    static const Stylesheet& userAgentStylesheet();
    // A built-in theme's CSS for light or dark, and CSS text as it is.
    static std::string expand(const std::string& cssOrTheme);
};

}  // namespace jadefx
