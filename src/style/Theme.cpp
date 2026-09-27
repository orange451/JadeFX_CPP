#include "jadefx/style/Theme.hpp"

#include <string>

namespace jadefx {
namespace {

// Every themed color once: its custom property and its value in each theme.
// A value may name another property with var().
struct PaletteEntry {
    ThemeColor color;
    const char* name;
    const char* light;
    const char* dark;
};

constexpr PaletteEntry kPalette[] = {
    {ThemeColor::Background, "--background-color", "#f8f8f8", "#202124"},
    {ThemeColor::Surface, "--surface-color", "#ffffff", "#292a2d"},
    {ThemeColor::Text, "--text-color", "#000000", "#e8eaed"},
    {ThemeColor::Muted, "--muted-color", "#5f6368", "#9aa0a6"},
    {ThemeColor::Faint, "--faint-color", "#80868b", "#80868b"},
    {ThemeColor::Border, "--border-color", "#dadce0", "#3c4043"},
    {ThemeColor::Accent, "--accent-color", "#1a73e8", "#8ab4f8"},
    {ThemeColor::Outline, "--outline-color", "var(--accent-color)", "var(--accent-color)"},
    {ThemeColor::Link, "--link-color", "#0b57d0", "#8ab4f8"},
    {ThemeColor::Hover, "--hover-color", "#e8eaed", "#3c4043"},
    {ThemeColor::Track, "--track-color", "#f1f3f4", "#35363a"},
    {ThemeColor::Subtle, "--subtle-color", "#f8f9fa", "#2d2e31"},
    {ThemeColor::Selection, "--selection-color", "#e8f0fe", "#394457"},
    {ThemeColor::SelectionHover, "--selection-hover-color", "#d2e3fc", "#43506a"},
    {ThemeColor::RowHover, "--row-hover-color", "#f5f5f5", "#313235"},
    {ThemeColor::Tab, "--tab-color", "var(--border-color)", "#35363a"},
    {ThemeColor::Tooltip, "--tooltip-color", "#3c4043", "#e8eaed"},
    {ThemeColor::TooltipText, "--tooltip-text-color", "#ffffff", "#202124"},
    {ThemeColor::Dimmer, "--dimmer-color", "rgba(0, 0, 0, 0.35)", "rgba(0, 0, 0, 0.55)"},
    {ThemeColor::Wash, "--wash-color", "rgba(0, 0, 0, 0.04)", "rgba(255, 255, 255, 0.06)"},
    {ThemeColor::TextSelection, "--text-selection-color", "rgba(26, 115, 232, 0.3)", "rgba(138, 180, 248, 0.35)"},
    {ThemeColor::CurrentLine, "--current-line-color", "rgba(0, 0, 0, 0.045)", "rgba(255, 255, 255, 0.05)"},
    {ThemeColor::Gutter, "--gutter-color", "rgba(0, 0, 0, 0.035)", "rgba(255, 255, 255, 0.04)"},
    {ThemeColor::Scrollbar, "--scrollbar-color", "rgba(0, 0, 0, 0.35)", "rgba(255, 255, 255, 0.35)"},
    {ThemeColor::Divider, "--divider-color", "#b0b0b0", "#3c4043"},
    {ThemeColor::Grip, "--grip-color", "#6e6e6e", "#9aa0a6"},
    {ThemeColor::Info, "--info-color", "var(--accent-color)", "var(--accent-color)"},
    {ThemeColor::Warning, "--warning-color", "#e37400", "#fdd663"},
    {ThemeColor::Error, "--error-color", "#d93025", "#f28b82"},
    {ThemeColor::Success, "--success-color", "#188038", "#81c995"},
};

// The rules both themes share. Colors come from the custom properties above;
// sizes a control also takes from code are left to the control.
constexpr const char* kRules = R"CSS(
scene {
    background-color: var(--background-color);
    color: var(--text-color);
}
button, togglebutton, radiobutton, menubutton, textfield, combobox, spinner, treeview, listview, table,
tabpane, menubar, combo-popup, menu-popup, alert, color-picker, color-chooser {
    background-color: var(--surface-color);
}
checkbox {
    background-color: transparent;
}
combobox, combo-popup, tooltip, color-picker {
    border-radius: 4px;
}
color-chooser {
    padding: 12px;
}
color-chooser:popover-open {
    border-radius: 6px;
    border-width: 1px;
    border-color: var(--border-color);
    box-shadow: 0px 3px 10px 0px rgba(0, 0, 0, 0.14);
}
color-chooser .caption {
    color: var(--muted-color);
    font-size: 12px;
}
menu-popup {
    border-radius: 4px;
    box-shadow: 0px 3px 10px 0px rgba(0, 0, 0, 0.14);
}
alert {
    border-radius: 8px;
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.3);
}
.alert-title.information {
    color: var(--info-color);
}
.alert-title.warning {
    color: var(--warning-color);
}
.alert-title.error {
    color: var(--error-color);
}
.alert-title.confirmation {
    color: var(--success-color);
}
.alert-dimmer {
    background-color: var(--dimmer-color);
}
tooltip {
    background-color: var(--tooltip-color);
    color: var(--tooltip-text-color);
}
.menu-accelerator, .menu-arrow {
    color: var(--muted-color);
}
progress-bar track {
    background-color: var(--track-color);
}
tab-header-area {
    background-color: var(--track-color);
}
tab {
    background-color: var(--tab-color);
}
tab:selected {
    background-color: var(--surface-color);
}
split-pane {
    background-color: var(--background-color);
}
split-pane-divider {
    background-color: var(--divider-color);
}
horizontal-grabber, vertical-grabber {
    background-color: var(--grip-color);
}
selection-bar, column-drag-marker {
    background-color: var(--accent-color);
}
tree-cell:hover, list-cell:hover, tr:hover {
    background-color: var(--row-hover-color);
}
tree-cell:selected, list-cell:selected, tr:selected {
    background-color: var(--selection-color);
}
tree-cell:selected:hover, list-cell:selected:hover, tr:selected:hover {
    background-color: var(--selection-hover-color);
}
list-cell:empty:hover, tr:empty:hover {
    background-color: transparent;
}
th, thead {
    background-color: var(--subtle-color);
}
)CSS";

const PaletteEntry* EntryFor(ThemeColor color) {
    for (const PaletteEntry& entry : kPalette) {
        if (entry.color == color) {
            return &entry;
        }
    }
    return nullptr;
}

// Set and read on the UI thread, like the rest of the scene graph.
struct Global {
    std::string source;
    Stylesheet sheet = Stylesheet::parse(Theme::stylesheet(Theme::LIGHT));
};

Global& TheGlobal() {
    static Global global;
    return global;
}

}  // namespace

const char* Theme::variableName(ThemeColor color) {
    const PaletteEntry* entry = EntryFor(color);
    return entry != nullptr ? entry->name : "";
}

Color Theme::defaultColor(ThemeColor color) {
    const PaletteEntry* entry = EntryFor(color);
    if (entry == nullptr) {
        return Color::black();
    }
    // A value that names another property is that property's light value.
    std::string value = entry->light;
    for (int depth = 0; depth < 4 && value.rfind("var(", 0) == 0; ++depth) {
        const std::string name = value.substr(4, value.find(')') - 4);
        value.clear();
        for (const PaletteEntry& other : kPalette) {
            if (name == other.name) {
                value = other.light;
            }
        }
    }
    return Color::parse(value);
}

std::string Theme::stylesheet(std::string_view name) {
    const bool dark = name == DARK;
    if (!dark && name != LIGHT) {
        return {};
    }
    std::string css = ":root {\n";
    for (const PaletteEntry& entry : kPalette) {
        css += "    ";
        css += entry.name;
        css += ": ";
        css += dark ? entry.dark : entry.light;
        css += ";\n";
    }
    css += "}\n";
    css += kRules;
    return css;
}

std::string Theme::expand(const std::string& cssOrTheme) {
    if (cssOrTheme.empty()) {
        return stylesheet(LIGHT);
    }
    std::string builtIn = stylesheet(cssOrTheme);
    return builtIn.empty() ? cssOrTheme : builtIn;
}

void Theme::setUserAgentStylesheet(std::string cssOrTheme) {
    Global& global = TheGlobal();
    global.sheet = Stylesheet::parse(expand(cssOrTheme));
    global.source = std::move(cssOrTheme);
}

const std::string& Theme::getUserAgentStylesheet() { return TheGlobal().source; }

const Stylesheet& Theme::userAgentStylesheet() { return TheGlobal().sheet; }

}  // namespace jadefx
