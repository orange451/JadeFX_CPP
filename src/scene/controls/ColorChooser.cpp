#include "jadefx/scene/controls/ColorChooser.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/controls/Slider.hpp"
#include "jadefx/scene/controls/Spinner.hpp"
#include "jadefx/scene/controls/TextField.hpp"
#include "jadefx/scene/layout/GridPane.hpp"
#include "jadefx/scene/layout/HBox.hpp"
#include "jadefx/scene/layout/VBox.hpp"
#include "scene/image/ImageData.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace jadefx {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kPaletteColumns = 12;
constexpr double kSwatch = 15;
constexpr double kSwatchGap = 2;
constexpr double kWheel = 200;
constexpr int kWheelPixels = 384;
constexpr float kGrooveThickness = 12.f;
// The number boxes and the hex field are a line of text with a little room, so the rows stay close.
const Insets kFieldPadding = Insets::axes(3, 6);

std::vector<Color> DefaultPresets() {
    std::vector<Color> colors;
    for (int i = 0; i < kPaletteColumns; ++i) {
        const float level = 1.f - static_cast<float>(i) / static_cast<float>(kPaletteColumns - 1);
        colors.push_back(Color::rgba(level, level, level, 1.f));
    }
    // Tints, the pure hue, then shades, one row each.
    const double shades[][2] = {{0.25, 1.0}, {0.5, 1.0}, {1.0, 1.0}, {1.0, 0.75}, {1.0, 0.5}};
    for (const auto& shade : shades) {
        for (int i = 0; i < kPaletteColumns; ++i) {
            colors.push_back(Color::hsb(360.0 * i / kPaletteColumns, shade[0], shade[1]));
        }
    }
    return colors;
}

// Hue around the circle, red at the right and counterclockwise as on a color wheel,
// saturation from the center out, at full brightness. The edge is antialiased.
const std::shared_ptr<ImageData>& WheelImage() {
    static const std::shared_ptr<ImageData> image = [] {
        auto data = std::make_shared<ImageData>();
        data->width = kWheelPixels;
        data->height = kWheelPixels;
        data->rgba.assign(static_cast<std::size_t>(kWheelPixels) * kWheelPixels * 4, 0);
        const double radius = kWheelPixels * 0.5;
        for (int row = 0; row < kWheelPixels; ++row) {
            for (int column = 0; column < kWheelPixels; ++column) {
                const double dx = (column + 0.5 - radius) / radius;
                const double dy = (row + 0.5 - radius) / radius;
                const double distance = std::sqrt(dx * dx + dy * dy);
                const double coverage = std::clamp((1.0 - distance) * radius + 0.5, 0.0, 1.0);
                if (coverage <= 0.0) {
                    continue;
                }
                const double hue = std::atan2(-dy, dx) * 180.0 / kPi;
                const Color color = Color::hsb(hue, std::min(1.0, distance), 1.0);
                unsigned char* pixel = &data->rgba[(static_cast<std::size_t>(row) * kWheelPixels + column) * 4];
                pixel[0] = static_cast<unsigned char>(std::lround(color.r * 255.f));
                pixel[1] = static_cast<unsigned char>(std::lround(color.g * 255.f));
                pixel[2] = static_cast<unsigned char>(std::lround(color.b * 255.f));
                pixel[3] = static_cast<unsigned char>(std::lround(coverage * 255.0));
            }
        }
        return data;
    }();
    return image;
}

void StrokeCircle(UiRenderer& renderer, float centerX, float centerY, float radius, float thickness, const Color& color) {
    const float corners[4] = {radius, radius, radius, radius};
    const float sides[4] = {thickness, thickness, thickness, thickness};
    renderer.strokeRounded(centerX - radius, centerY - radius, radius * 2.f, radius * 2.f, corners, sides, color);
}

// Hue and saturation on a disc. Brightness dims the disc, and the ring marks the color.
// The arrow keys turn the hue (Left and Right) and move the saturation (Up and Down).
class ColorWheel : public Controls {
public:
    ColorWheel() {
        setDefaultCursor(Cursor::Crosshair);
        setPrefSize(kWheel, kWheel);
    }

    const char* getElementType() const override { return "color-wheel"; }

    void show(double hue, double saturation, double brightness) {
        hue_ = hue;
        saturation_ = saturation;
        brightness_ = brightness;
    }
    void setOnPick(std::function<void(double hue, double saturation)> handler) { onPick_ = std::move(handler); }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        float centerX = 0.f;
        float centerY = 0.f;
        const float radius = disc(centerX, centerY);
        if (radius <= 0.f) {
            return;
        }
        renderer.drawImage(WheelImage(), centerX - radius, centerY - radius, radius * 2.f, radius * 2.f, opacity);
        const float corners[4] = {radius, radius, radius, radius};
        const float at = 0.f;
        const Color dim = Color::rgba(0.f, 0.f, 0.f, static_cast<float>(1.0 - brightness_) * opacity);
        renderer.fillRounded(centerX - radius, centerY - radius, radius * 2.f, radius * 2.f, corners, &dim, &at, 1, 0.f);
        StrokeCircle(renderer, centerX, centerY, radius, 1.f, chrome::Themed(*this, ThemeColor::Border, opacity));

        const double angle = hue_ * kPi / 180.0;
        const float markX = centerX + static_cast<float>(std::cos(angle) * saturation_) * radius;
        const float markY = centerY - static_cast<float>(std::sin(angle) * saturation_) * radius;
        StrokeCircle(renderer, markX, markY, 6.f, 1.f, Color::rgba(0.f, 0.f, 0.f, 0.6f * opacity));
        StrokeCircle(renderer, markX, markY, 5.f, 2.f, Color::rgba(1.f, 1.f, 1.f, opacity));
        if (isFocused()) {
            StrokeCircle(renderer, centerX, centerY, radius + 2.f, 2.f, chrome::Themed(*this, ThemeColor::Outline, opacity));
        }
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (event.button == 0) {
            requestFocus();
            pickAt(event.x, event.y);
        }
    }

    void handleMouseDragged(const MouseEvent& event) override { pickAt(event.x, event.y); }

    void handleKey(KeyEvent& event) override {
        if (!event.pressed && !event.repeat) {
            return;
        }
        const double step = event.shift ? 10.0 : 1.0;
        double hue = hue_;
        double saturation = saturation_;
        switch (event.key) {
            case Key::Left: hue = std::fmod(hue_ + step + 360.0, 360.0); break;
            case Key::Right: hue = std::fmod(hue_ - step + 360.0, 360.0); break;
            case Key::Up: saturation = std::min(1.0, saturation_ + step / 100.0); break;
            case Key::Down: saturation = std::max(0.0, saturation_ - step / 100.0); break;
            default: return;
        }
        event.consume();
        if (onPick_) {
            onPick_(hue, saturation);
        }
    }

private:
    // The disc's center and radius in window points: the largest circle in the content box.
    float disc(float& centerX, float& centerY) const {
        const float width = static_cast<float>(contentWidth());
        const float height = static_cast<float>(contentHeight());
        centerX = static_cast<float>(getAbsoluteX() + contentLeft()) + width * 0.5f;
        centerY = static_cast<float>(getAbsoluteY() + contentTop()) + height * 0.5f;
        return std::max(0.f, std::min(width, height) * 0.5f);
    }

    void pickAt(double x, double y) {
        float centerX = 0.f;
        float centerY = 0.f;
        const float radius = disc(centerX, centerY);
        if (radius <= 0.f || !onPick_) {
            return;
        }
        const double dx = (x - centerX) / radius;
        const double dy = (y - centerY) / radius;
        const double distance = std::min(1.0, std::sqrt(dx * dx + dy * dy));
        double hue = std::atan2(-dy, dx) * 180.0 / kPi;
        if (hue < 0.0) {
            hue += 360.0;
        }
        // At the center the angle is noise, so the hue stays.
        onPick_(distance > 0.0 ? hue : hue_, distance);
    }

    double hue_ = 0;
    double saturation_ = 0;
    double brightness_ = 1;
    std::function<void(double, double)> onPick_;
};

// A slider whose groove shows the colors it runs through, over a checkerboard for alpha.
class ColorSlider : public Slider {
public:
    explicit ColorSlider(double max) : Slider(0, max, 0) {
        getClassList().add("color-slider");
        setBlockIncrement(max / 20.0);
    }

    void setStops(std::vector<Color> stops) { stops_ = std::move(stops); }
    void setCheckered(bool checkered) { checkered_ = checkered; }

protected:
    void renderGroove(UiRenderer& renderer, float opacity, const Groove& groove) override {
        if (stops_.empty()) {
            Slider::renderGroove(renderer, opacity, groove);
            return;
        }
        const float half = kGrooveThickness * 0.5f;
        const float x = groove.horizontal ? groove.start : groove.cross - half;
        const float y = groove.horizontal ? groove.cross - half : groove.start;
        const float width = groove.horizontal ? groove.length : kGrooveThickness;
        const float height = groove.horizontal ? kGrooveThickness : groove.length;
        if (checkered_) {
            chrome::DrawCheckerboard(renderer, x, y, width, height, opacity, half);
        }
        const std::size_t count = std::min<std::size_t>(stops_.size(), 8);
        std::vector<Color> colors(stops_.begin(), stops_.begin() + static_cast<std::ptrdiff_t>(count));
        std::vector<float> at(count, 0.f);
        for (std::size_t i = 0; i < count; ++i) {
            colors[i].a *= opacity;
            at[i] = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.f;
        }
        const float corners[4] = {3.f, 3.f, 3.f, 3.f};
        // Toward max: right on a horizontal slider, up on a vertical one.
        renderer.fillRounded(x, y, width, height, corners, colors.data(), at.data(), static_cast<int>(count),
                             groove.horizontal ? 90.f : 0.f);
        const float sides[4] = {1.f, 1.f, 1.f, 1.f};
        renderer.strokeRounded(x, y, width, height, corners, sides, chrome::Themed(*this, ThemeColor::Border, opacity));
    }

private:
    std::vector<Color> stops_;
    bool checkered_ = false;
};

// One color to click, or an empty slot. The selected swatch has a ring.
class Swatch : public Controls {
public:
    Swatch(double width, double height) {
        setPrefSize(width, height);
        setFocusTraversable(false);
    }

    const char* getElementType() const override { return "color-swatch"; }

    void setColor(std::optional<Color> color) {
        color_ = color;
        syncCursor();
    }
    const std::optional<Color>& getColor() const { return color_; }
    void setSelected(bool selected) { setPseudoState("checked", selected); }
    void setOnAction(std::function<void(const Color&)> handler) {
        onAction_ = std::move(handler);
        syncCursor();
    }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (!color_) {
            const float corners[4] = {2.f, 2.f, 2.f, 2.f};
            const float sides[4] = {1.f, 1.f, 1.f, 1.f};
            renderer.strokeRounded(x, y, width, height, corners, sides, chrome::Themed(*this, ThemeColor::Border, opacity));
            return;
        }
        chrome::DrawColorSwatch(renderer, *this, x, y, width, height, *color_, opacity);
        const bool checked = pseudoState("checked");
        if (checked || isHovered()) {
            const float corners[4] = {3.f, 3.f, 3.f, 3.f};
            const float sides[4] = {2.f, 2.f, 2.f, 2.f};
            const Color ring = chrome::Themed(*this, checked ? ThemeColor::Text : ThemeColor::Accent, opacity);
            renderer.strokeRounded(x - 1.f, y - 1.f, width + 2.f, height + 2.f, corners, sides, ring);
        }
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (event.button == 0 && color_ && onAction_) {
            onAction_(*color_);
        }
    }

private:
    // A pointer over a swatch that does something when clicked.
    void syncCursor() { setDefaultCursor(color_ && onAction_ ? Cursor::Pointer : Cursor::Default); }

    std::optional<Color> color_;
    std::function<void(const Color&)> onAction_;
};

std::shared_ptr<Label> Caption(const char* text) {
    auto label = std::make_shared<Label>(text);
    label->getClassList().add("caption");
    return label;
}

std::string Trimmed(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

}  // namespace

struct ColorChooser::Parts {
    struct Row {
        Channel channel = Channel::Red;
        std::shared_ptr<ColorSlider> slider;
        std::shared_ptr<Spinner> spinner;
        std::shared_ptr<IntegerSpinnerValueFactory> factory;
    };

    std::shared_ptr<HBox> root;
    std::shared_ptr<ColorWheel> wheel;
    std::shared_ptr<Swatch> before;
    std::shared_ptr<Swatch> after;
    std::shared_ptr<GridPane> presetGrid;
    std::shared_ptr<GridPane> recentGrid;
    std::vector<std::shared_ptr<Swatch>> presetSwatches;
    std::vector<std::shared_ptr<Swatch>> recentSwatches;
    std::vector<Row> rows;
    std::shared_ptr<TextField> hex;
    // The transparency caption and the alpha row, hidden without alpha.
    std::vector<std::shared_ptr<Node>> alphaNodes;
    std::shared_ptr<VBox> left;
    // The recent caption and swatches, taken out of the left column when hidden.
    std::shared_ptr<VBox> recentBox;
};

ColorChooser::ColorChooser() : ColorChooser(Color::white()) {}

ColorChooser::ColorChooser(Color initial) : parts_(std::make_unique<Parts>()) {
    // Its fields take the focus. A press on the panel between them leaves it where it was.
    setFocusTraversable(false);
    for (const Color& color : DefaultPresets()) {
        presets_.add(color);
    }
    presets_.addListener([this](const ObservableList<Color>::Change&) { rebuildPresets(); });
    recent_.addListener([this](const ObservableList<Color>::Change&) { refresh(); });
    build();
    original_ = initial;
    applyColor(initial, false);
}

ColorChooser::~ColorChooser() = default;

void ColorChooser::build() {
    Parts& parts = *parts_;
    auto channelMax = [](Channel channel) {
        switch (channel) {
            case Channel::Hue: return 360;
            case Channel::Saturation:
            case Channel::Brightness: return 100;
            default: return 255;
        }
    };

    parts.wheel = std::make_shared<ColorWheel>();
    parts.wheel->setOnPick([this](double hue, double saturation) { applyHsb(hue, saturation, brightness_, true); });

    parts.before = std::make_shared<Swatch>(kWheel * 0.5, 24);
    parts.before->setOnAction([this](const Color& color) { applyColor(color, true); });
    parts.after = std::make_shared<Swatch>(kWheel * 0.5, 24);
    auto compare = std::make_shared<HBox>();
    compare->getChildren().add(parts.before);
    compare->getChildren().add(parts.after);

    parts.presetGrid = std::make_shared<GridPane>();
    parts.presetGrid->setHgap(kSwatchGap);
    parts.presetGrid->setVgap(kSwatchGap);
    parts.recentGrid = std::make_shared<GridPane>();
    parts.recentGrid->setHgap(kSwatchGap);

    parts.recentBox = std::make_shared<VBox>();
    parts.recentBox->setSpacing(6);
    parts.recentBox->getChildren().add(Caption("Recent"));
    parts.recentBox->getChildren().add(parts.recentGrid);
    parts.left = std::make_shared<VBox>();
    VBox& left = *parts.left;
    left.setSpacing(6);
    left.getChildren().add(parts.wheel);
    left.getChildren().add(compare);
    left.getChildren().add(Caption("Presets"));
    left.getChildren().add(parts.presetGrid);
    left.getChildren().add(parts.recentBox);

    auto grid = std::make_shared<GridPane>();
    grid->setHgap(6);
    grid->setVgap(4);
    ColumnConstraints labels;
    labels.minWidth = 28;
    ColumnConstraints sliders;
    sliders.hgrow = Priority::Always;
    sliders.fillWidth = true;
    sliders.minWidth = 160;
    grid->getColumnConstraints() = {labels, sliders, ColumnConstraints()};

    int row = 0;
    auto header = [&](const char* text) {
        auto caption = Caption(text);
        grid->add(caption, 0, row++, GridPane::REMAINING, 1);
        return caption;
    };
    // Each channel's slider and number box carry its name as a style class, such as .red or .alpha.
    auto channel = [&](Channel kind, const char* name, const char* styleClass) {
        Parts::Row entry;
        entry.channel = kind;
        const int max = channelMax(kind);
        entry.slider = std::make_shared<ColorSlider>(max);
        entry.slider->getClassList().add(styleClass);
        entry.slider->setCheckered(kind == Channel::Alpha);
        const std::size_t index = parts_->rows.size();
        entry.slider->setOnValueChanged([this, index] {
            if (!syncing_) {
                const Parts::Row& changed = parts_->rows[index];
                applyChannel(changed.channel, changed.slider->getValue());
            }
        });
        entry.factory = std::make_shared<IntegerSpinnerValueFactory>(0, max, 0);
        entry.spinner = std::make_shared<Spinner>(entry.factory);
        entry.spinner->setEditable(true);
        entry.spinner->setPrefWidth(64);
        entry.spinner->setPadding(kFieldPadding);
        entry.spinner->getClassList().add(styleClass);
        entry.spinner->setOnValueChanged([this, index] {
            if (!syncing_) {
                const Parts::Row& changed = parts_->rows[index];
                applyChannel(changed.channel, changed.factory->getValue());
            }
        });
        auto label = std::make_shared<Label>(name);
        grid->add(label, 0, row);
        grid->add(entry.slider, 1, row);
        grid->add(entry.spinner, 2, row);
        ++row;
        if (kind == Channel::Alpha) {
            parts_->alphaNodes = {label, entry.slider, entry.spinner};
        }
        parts_->rows.push_back(std::move(entry));
    };

    header("RGB");
    channel(Channel::Red, "R", "red");
    channel(Channel::Green, "G", "green");
    channel(Channel::Blue, "B", "blue");
    parts.hex = std::make_shared<TextField>();
    parts.hex->setPrefColumnCount(9);
    parts.hex->setPadding(kFieldPadding);
    parts.hex->getClassList().add("hex");
    parts.hex->setOnAction([this](ActionEvent&) {
        if (!applyHex(parts_->hex->getText())) {
            refresh();
        }
    });
    parts.hex->setOnFocusChanged([this](bool focused) {
        if (!focused && !applyHex(parts_->hex->getText())) {
            refresh();
        }
    });
    grid->add(std::make_shared<Label>("Hex"), 0, row);
    grid->add(parts.hex, 1, row, GridPane::REMAINING, 1);
    ++row;
    header("HSV");
    channel(Channel::Hue, "H", "hue");
    channel(Channel::Saturation, "S", "saturation");
    channel(Channel::Brightness, "V", "brightness");
    auto transparency = header("Transparency");
    channel(Channel::Alpha, "A", "alpha");
    parts.alphaNodes.push_back(transparency);

    for (std::size_t i = 0; i < kRecentLimit; ++i) {
        auto swatch = std::make_shared<Swatch>(kSwatch, kSwatch);
        swatch->setOnAction([this](const Color& color) { applyColor(color, true); });
        parts.recentGrid->add(swatch, static_cast<int>(i), 0);
        parts.recentSwatches.push_back(std::move(swatch));
    }

    parts.root = std::make_shared<HBox>();
    parts.root->setSpacing(14);
    parts.root->getChildren().add(parts.left);
    parts.root->getChildren().add(grid);
    children().add(parts.root);
    rebuildPresets();
}

void ColorChooser::rebuildPresets() {
    Parts& parts = *parts_;
    parts.presetGrid->getChildren().clear();
    parts.presetSwatches.clear();
    for (std::size_t i = 0; i < presets_.size(); ++i) {
        auto swatch = std::make_shared<Swatch>(kSwatch, kSwatch);
        swatch->setColor(presets_[i]);
        swatch->setOnAction([this](const Color& color) { applyColor(color, true); });
        parts.presetGrid->add(swatch, static_cast<int>(i) % kPaletteColumns, static_cast<int>(i) / kPaletteColumns);
        parts.presetSwatches.push_back(std::move(swatch));
    }
    refresh();
}

void ColorChooser::setValue(Color color) { applyColor(color, false); }

void ColorChooser::setShowAlpha(bool show) {
    showAlpha_ = show;
    for (const std::shared_ptr<Node>& node : parts_->alphaNodes) {
        node->setVisible(show);
    }
    if (!show && value_.a < 1.f) {
        value_.a = 1.f;
    }
    if (!show) {
        original_.a = 1.f;
    }
    refresh();
}

void ColorChooser::setShowRecentColors(bool show) {
    if (show == showRecent_) {
        return;
    }
    showRecent_ = show;
    ObservableList<std::shared_ptr<Node>>& column = parts_->left->getChildren();
    if (show) {
        column.add(parts_->recentBox);
    } else {
        column.removeIf([&](const std::shared_ptr<Node>& node) { return node == parts_->recentBox; });
    }
}

void ColorChooser::setOriginalValue(Color color) {
    original_ = color;
    refresh();
}

void ColorChooser::addRecentColor(Color color) {
    recent_.removeIf([&](const Color& existing) { return near(existing, color); });
    recent_.insert(0, color);
    while (recent_.size() > kRecentLimit) {
        recent_.removeAt(recent_.size() - 1);
    }
}

void ColorChooser::commitEdits() {
    for (const Parts::Row& row : parts_->rows) {
        row.spinner->commitValue();
    }
    const std::string text = parts_->hex->getText();
    if (text != (value_.a < 1.f ? value_.toHex(true) : value_.toHex()) && !applyHex(text)) {
        refresh();
    }
}

void ColorChooser::applyColor(Color color, bool notify) {
    // Without alpha the chooser picks opaque colors only, as an RGB value has no transparency.
    if (!showAlpha_) {
        color.a = 1.f;
    }
    value_ = color;
    const double brightness = color.getBrightness();
    const double saturation = color.getSaturation();
    // Black has no saturation and a gray no hue, so those keep what they were.
    if (brightness > 0.0) {
        if (saturation > 0.0) {
            hue_ = color.getHue();
        }
        saturation_ = saturation;
    }
    brightness_ = brightness;
    changed(notify);
}

void ColorChooser::changed(bool notify) {
    refresh();
    if (notify && onChanged_) {
        onChanged_();
    }
}

void ColorChooser::applyHsb(double hue, double saturation, double brightness, bool notify) {
    hue_ = std::clamp(hue, 0.0, 360.0);
    saturation_ = std::clamp(saturation, 0.0, 1.0);
    brightness_ = std::clamp(brightness, 0.0, 1.0);
    value_ = Color::hsb(hue_, saturation_, brightness_, value_.a);
    changed(notify);
}

void ColorChooser::applyChannel(Channel channel, double value) {
    const float byte = static_cast<float>(std::clamp(std::round(value), 0.0, 255.0) / 255.0);
    Color color = value_;
    switch (channel) {
        case Channel::Red: color.r = byte; break;
        case Channel::Green: color.g = byte; break;
        case Channel::Blue: color.b = byte; break;
        case Channel::Alpha: color.a = byte; break;
        case Channel::Hue: applyHsb(value, saturation_, brightness_, true); return;
        case Channel::Saturation: applyHsb(hue_, value / 100.0, brightness_, true); return;
        case Channel::Brightness: applyHsb(hue_, saturation_, value / 100.0, true); return;
    }
    // Alpha leaves hue, saturation, and brightness exactly where they are.
    if (channel == Channel::Alpha) {
        value_ = color;
        changed(true);
        return;
    }
    applyColor(color, true);
}

bool ColorChooser::applyHex(const std::string& text) {
    std::string hex = Trimmed(text);
    if (!hex.empty() && hex[0] != '#') {
        hex.insert(hex.begin(), '#');
    }
    const std::size_t digits = hex.size() - 1;
    if (digits != 3 && digits != 4 && digits != 6 && digits != 8) {
        return false;
    }
    for (std::size_t i = 1; i < hex.size(); ++i) {
        if (std::isxdigit(static_cast<unsigned char>(hex[i])) == 0) {
            return false;
        }
    }
    bool ok = false;
    Color color = Color::parse(hex, &ok);
    if (!ok) {
        return false;
    }
    // Without an alpha digit the color keeps its transparency. Without alpha, applyColor makes it opaque.
    if (digits == 3 || digits == 6) {
        color.a = value_.a;
    }
    if (near(color, value_)) {
        refresh();
        return true;
    }
    applyColor(color, true);
    return true;
}

void ColorChooser::refresh() {
    Parts& parts = *parts_;
    if (parts.root == nullptr) {
        return;
    }
    syncing_ = true;
    parts.wheel->show(hue_, saturation_, brightness_);
    parts.before->setColor(original_);
    parts.after->setColor(value_);
    const Color opaque = Color::rgba(value_.r, value_.g, value_.b, 1.f);
    for (Parts::Row& row : parts.rows) {
        double value = 0;
        std::vector<Color> stops;
        switch (row.channel) {
            case Channel::Red:
                value = value_.r * 255.0;
                stops = {Color::rgba(0.f, opaque.g, opaque.b, 1.f), Color::rgba(1.f, opaque.g, opaque.b, 1.f)};
                break;
            case Channel::Green:
                value = value_.g * 255.0;
                stops = {Color::rgba(opaque.r, 0.f, opaque.b, 1.f), Color::rgba(opaque.r, 1.f, opaque.b, 1.f)};
                break;
            case Channel::Blue:
                value = value_.b * 255.0;
                stops = {Color::rgba(opaque.r, opaque.g, 0.f, 1.f), Color::rgba(opaque.r, opaque.g, 1.f, 1.f)};
                break;
            case Channel::Hue:
                value = hue_;
                for (int i = 0; i <= 6; ++i) {
                    stops.push_back(Color::hsb(60.0 * i, 1.0, 1.0));
                }
                break;
            case Channel::Saturation:
                value = saturation_ * 100.0;
                stops = {Color::hsb(hue_, 0.0, brightness_), Color::hsb(hue_, 1.0, brightness_)};
                break;
            case Channel::Brightness:
                value = brightness_ * 100.0;
                stops = {Color::black(), Color::hsb(hue_, saturation_, 1.0)};
                break;
            case Channel::Alpha:
                value = value_.a * 255.0;
                stops = {Color::rgba(opaque.r, opaque.g, opaque.b, 0.f), opaque};
                break;
        }
        row.slider->setStops(std::move(stops));
        if (!row.slider->isValueChanging()) {
            row.slider->setValue(value);
        }
        row.factory->setValue(static_cast<int>(std::lround(value)));
    }
    if (!parts.hex->isFocused()) {
        parts.hex->setText(value_.a < 1.f ? value_.toHex(true) : value_.toHex());
    }
    for (const std::shared_ptr<Swatch>& swatch : parts.presetSwatches) {
        swatch->setSelected(swatch->getColor() && near(*swatch->getColor(), value_));
    }
    for (std::size_t i = 0; i < parts.recentSwatches.size(); ++i) {
        parts.recentSwatches[i]->setColor(i < recent_.size() ? std::optional<Color>(recent_[i]) : std::nullopt);
    }
    syncing_ = false;
}

void ColorChooser::layoutChildren() {
    if (parts_->root != nullptr) {
        parts_->root->performLayout(contentLeft(), contentTop(), contentWidth(), contentHeight());
    }
}

double ColorChooser::preferredContentWidth(double innerAvailable) const {
    return parts_->root != nullptr ? parts_->root->measuredWidth(innerAvailable) : 0.0;
}

double ColorChooser::preferredContentHeight(double innerWidth) const {
    return parts_->root != nullptr ? parts_->root->measuredHeight(innerWidth, -1) : 0.0;
}

}  // namespace jadefx
