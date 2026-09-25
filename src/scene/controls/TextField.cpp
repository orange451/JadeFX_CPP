#include "jadefx/scene/controls/TextField.hpp"

#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"
#include "scene/text/Unicode.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace jadefx {
namespace {

enum class UnitKind { Space, Alnum, Other };

// Letters and digits form one kind of word. Other non-space characters form another.
UnitKind Classify(char32_t codepoint) {
    if (codepoint == U' ' || codepoint == U'\t' || codepoint == U'\n' || codepoint == U'\r' || codepoint == U'\f' ||
        codepoint == U'\v' || codepoint == 0x00A0) {
        return UnitKind::Space;
    }
    if (codepoint < 128) {
        const bool digit = codepoint >= U'0' && codepoint <= U'9';
        const bool letter = (codepoint >= U'A' && codepoint <= U'Z') || (codepoint >= U'a' && codepoint <= U'z');
        return (digit || letter) ? UnitKind::Alnum : UnitKind::Other;
    }
    return UnitKind::Alnum;
}

// Land on the next word and step over the spaces between words.
int NextWord(const std::u32string& chars, int index) {
    const int length = static_cast<int>(chars.size());
    if (index < 0) {
        index = 0;
    }
    if (index >= length) {
        return length;
    }
    const UnitKind kind = Classify(chars[static_cast<std::size_t>(index)]);
    if (kind != UnitKind::Space) {
        while (index < length && Classify(chars[static_cast<std::size_t>(index)]) == kind) {
            ++index;
        }
    }
    while (index < length && Classify(chars[static_cast<std::size_t>(index)]) == UnitKind::Space) {
        ++index;
    }
    return index;
}

int PreviousWord(const std::u32string& chars, int index) {
    const int length = static_cast<int>(chars.size());
    if (index > length) {
        index = length;
    }
    if (index <= 0) {
        return 0;
    }
    int cursor = index - 1;
    while (cursor > 0 && Classify(chars[static_cast<std::size_t>(cursor)]) == UnitKind::Space) {
        --cursor;
    }
    const UnitKind kind = Classify(chars[static_cast<std::size_t>(cursor)]);
    if (kind == UnitKind::Space) {
        return 0;
    }
    while (cursor > 0 && Classify(chars[static_cast<std::size_t>(cursor - 1)]) == kind) {
        --cursor;
    }
    return cursor;
}

int ClampIndex(int index, int length) {
    if (length < 0) {
        length = 0;
    }
    if (index < 0) {
        return 0;
    }
    if (index > length) {
        return length;
    }
    return index;
}

// The field is one line, so breaks never become part of the text.
std::string StripLineBreaks(std::string text) {
    std::string out;
    out.reserve(text.size());
    for (char unit : text) {
        if (unit != '\n' && unit != '\r') {
            out.push_back(unit);
        }
    }
    return out;
}

float AlignIn(float view, float content, HPos horizontal) {
    if (!(content <= view)) {
        return 0.f;
    }
    if (horizontal == HPos::Center) {
        return (view - content) * 0.5f;
    }
    if (horizontal == HPos::Right) {
        return view - content;
    }
    return 0.f;
}

}  // namespace

struct TextField::LineLayout {
    Font font;
    std::vector<float> caretX;
    float textWidth = 0.f;
    float lineHeight = 0.f;
    float origin = 0.f;
    float top = 0.f;
};

TextField::TextField() : TextField(std::string()) {}

TextField::TextField(std::string text) {
    setAlignment(Pos::CenterLeft);
    setPadding(Insets::axes(6, 8));
    setBackground(Color::white());
    setDefaultCursor(Cursor::Text);
    setText(std::move(text));
}

void TextField::setText(std::string text) {
    text_ = StripLineBreaks(std::move(text));
    const int length = getLength();
    caret_ = ClampIndex(caret_, length);
    anchor_ = caret_;
    ensureCaretVisible();
}

int TextField::getLength() const { return static_cast<int>(Utf32(text_).size()); }

void TextField::positionCaret(int index) {
    const int length = getLength();
    caret_ = ClampIndex(index, length);
    anchor_ = caret_;
    ensureCaretVisible();
}

void TextField::selectRange(int anchor, int caret) {
    const int length = getLength();
    anchor_ = ClampIndex(anchor, length);
    caret_ = ClampIndex(caret, length);
    ensureCaretVisible();
}

void TextField::selectAll() {
    anchor_ = 0;
    caret_ = getLength();
    ensureCaretVisible();
}

void TextField::deselect() {
    const int length = getLength();
    caret_ = ClampIndex(caret_, length);
    anchor_ = caret_;
}

void TextField::clear() { setText({}); }

std::string TextField::getSelectedText() const {
    const std::u32string chars = Utf32(text_);
    const int length = static_cast<int>(chars.size());
    const int from = ClampIndex(std::min(anchor_, caret_), length);
    const int to = ClampIndex(std::max(anchor_, caret_), length);
    if (from >= to) {
        return {};
    }
    return Utf8(chars.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from)));
}

void TextField::replaceSelection(std::string text) {
    const std::u32string insert = Utf32(StripLineBreaks(std::move(text)));
    std::u32string chars = Utf32(text_);
    const int length = static_cast<int>(chars.size());
    const int from = ClampIndex(std::min(anchor_, caret_), length);
    const int to = ClampIndex(std::max(anchor_, caret_), length);
    chars.replace(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from), insert);
    text_ = Utf8(chars);
    caret_ = from + static_cast<int>(insert.size());
    anchor_ = caret_;
    ensureCaretVisible();
}

void TextField::cut() {
    if (!editable_) {
        return;
    }
    const std::string selected = getSelectedText();
    if (selected.empty()) {
        return;
    }
    writeClipboard(selected);
    replaceSelection({});
}

void TextField::copy() {
    const std::string selected = getSelectedText();
    if (selected.empty()) {
        return;
    }
    writeClipboard(selected);
}

void TextField::paste() {
    if (!editable_) {
        return;
    }
    replaceSelection(readClipboard());
}

void TextField::fire() {
    if (!onAction_) {
        return;
    }
    ActionEvent event;
    event.source = this;
    onAction_(event);
}

bool TextField::caretBounds(double& x, double& y, double& height) {
    x = 0;
    y = 0;
    height = 0;
    if (!(getWidth() > 0.0) || !(getHeight() > 0.0)) {
        return false;
    }
    ensureCaretVisible();
    const LineLayout line = measureLine();
    if (line.caretX.empty() || !(line.lineHeight > 0.f)) {
        return false;
    }
    const int length = static_cast<int>(line.caretX.size()) - 1;
    const int caret = ClampIndex(caret_, length);
    x = getAbsoluteX() + contentLeft() + static_cast<double>(line.origin) +
        static_cast<double>(line.caretX[static_cast<std::size_t>(caret)]);
    y = getAbsoluteY() + contentTop() + static_cast<double>(line.top);
    height = line.lineHeight;
    return true;
}

Font TextField::face() const {
    const ComputedStyle& style = computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    return Font(style.fontFamily.empty() ? std::string("Open Sans") : style.fontFamily, size);
}

TextField::LineLayout TextField::measureLine() const {
    LineLayout line;
    line.font = face();
    line.lineHeight = line.font.lineHeight();
    if (!(line.lineHeight > 0.f)) {
        line.lineHeight = line.font.size() > 0.f ? line.font.size() : 16.f;
    }
    const std::u32string chars = Utf32(text_);
    line.caretX.reserve(chars.size() + 1);
    if (chars.empty()) {
        line.caretX.push_back(0.f);
        line.textWidth = 0.f;
    } else {
        const ShapedText shaped = line.font.shape(text_);
        if (shaped.glyphs.size() == chars.size()) {
            for (const ShapedGlyph& glyph : shaped.glyphs) {
                line.caretX.push_back(glyph.x);
            }
            line.textWidth = shaped.width;
            line.caretX.push_back(shaped.width);
        } else {
            line.caretX.push_back(0.f);
            std::u32string prefix;
            prefix.reserve(chars.size());
            for (char32_t codepoint : chars) {
                prefix.push_back(codepoint);
                line.caretX.push_back(line.font.measureWidth(Utf8(prefix)));
            }
            line.textWidth = line.caretX.back();
        }
    }
    const float view = static_cast<float>(std::max(0.0, contentWidth()));
    const float boxH = static_cast<float>(std::max(0.0, contentHeight()));
    const HPos horizontal = hpos(usingAlignment());
    const float scroll = line.textWidth <= view ? 0.f : scroll_;
    line.origin = AlignIn(view, line.textWidth, horizontal) - scroll;
    line.top = boxH > line.lineHeight ? (boxH - line.lineHeight) * 0.5f : 0.f;
    return line;
}

void TextField::ensureCaretVisible() {
    const float view = static_cast<float>(std::max(0.0, contentWidth()));
    if (!(view > 0.f)) {
        scroll_ = 0.f;
        return;
    }
    const LineLayout line = measureLine();
    if (line.caretX.empty()) {
        scroll_ = 0.f;
        return;
    }
    const int caret = ClampIndex(caret_, static_cast<int>(line.caretX.size()) - 1);
    const float caretAt = line.caretX[static_cast<std::size_t>(caret)];
    // A line that fits keeps its alignment and does not scroll. Once it overflows, one
    // pixel is held back so the caret bar stays inside the content box.
    if (line.textWidth <= view) {
        scroll_ = 0.f;
        return;
    }
    const float inset = view > 1.f ? 1.f : 0.f;
    const float window = view - inset;
    if (caretAt < scroll_) {
        scroll_ = caretAt;
    } else if (caretAt > scroll_ + window) {
        scroll_ = caretAt - window;
    }
    const float maxScroll = std::max(0.f, line.textWidth - window);
    if (scroll_ < 0.f) {
        scroll_ = 0.f;
    }
    if (scroll_ > maxScroll) {
        scroll_ = maxScroll;
    }
}

int TextField::indexAt(double absoluteX) {
    ensureCaretVisible();
    const LineLayout line = measureLine();
    if (line.caretX.empty()) {
        return 0;
    }
    const float local = static_cast<float>(absoluteX - getAbsoluteX() - contentLeft()) - line.origin;
    int best = 0;
    float bestDistance = std::fabs(line.caretX[0] - local);
    for (int index = 1; index < static_cast<int>(line.caretX.size()); ++index) {
        const float distance = std::fabs(line.caretX[static_cast<std::size_t>(index)] - local);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

void TextField::moveCaret(int direction, bool extend, bool byWord) {
    const std::u32string chars = Utf32(text_);
    const int length = static_cast<int>(chars.size());
    const int caret = ClampIndex(caret_, length);
    int next = caret;
    if (byWord) {
        next = direction < 0 ? PreviousWord(chars, caret) : NextWord(chars, caret);
    } else if (direction < 0) {
        next = caret > 0 ? caret - 1 : 0;
    } else if (caret < length) {
        next = caret + 1;
    }
    caret_ = next;
    if (!extend) {
        anchor_ = next;
    }
    ensureCaretVisible();
}

void TextField::moveTo(int index, bool extend) {
    caret_ = ClampIndex(index, getLength());
    if (!extend) {
        anchor_ = caret_;
    }
    ensureCaretVisible();
}

void TextField::eraseOne(bool forward) {
    if (anchor_ != caret_) {
        replaceSelection({});
        return;
    }
    const int length = getLength();
    const int caret = ClampIndex(caret_, length);
    if (!forward) {
        if (caret == 0) {
            return;
        }
        anchor_ = caret - 1;
        caret_ = caret;
    } else {
        if (caret >= length) {
            return;
        }
        anchor_ = caret;
        caret_ = caret + 1;
    }
    replaceSelection({});
}

std::string TextField::readClipboard() const {
    if (Scene* scene = getScene()) {
        return scene->clipboardText();
    }
    return clipboard_;
}

void TextField::writeClipboard(std::string text) {
    if (Scene* scene = getScene()) {
        scene->setClipboardText(std::move(text));
        return;
    }
    clipboard_ = std::move(text);
}

void TextField::handleMousePressed(const MouseEvent& event) {
    if (isDisabled()) {
        return;
    }
    const int index = indexAt(event.x);
    anchor_ = index;
    caret_ = index;
    ensureCaretVisible();
}

void TextField::handleMouseDragged(const MouseEvent& event) {
    if (isDisabled()) {
        return;
    }
    caret_ = indexAt(event.x);
    ensureCaretVisible();
}

void TextField::handleKey(KeyEvent& event) {
    if (!event.pressed || isDisabled()) {
        return;
    }
    // A parent combo box handles these. Leaving them unconsumed is the point.
    if (event.key == Key::Up || event.key == Key::Down || event.key == Key::Escape || event.key == Key::Tab) {
        return;
    }
    const bool arrow = event.key == Key::Left || event.key == Key::Right;
    const bool erase = event.key == Key::Backspace || event.key == Key::Delete;
    // Shortcuts ignore auto-repeat. Backspace, Delete, and the arrows do not.
    if (event.repeat && !arrow && !erase) {
        return;
    }
    if (arrow) {
        moveCaret(event.key == Key::Left ? -1 : 1, event.shift, event.alt || event.shortcut());
        event.consume();
        return;
    }
    if (event.key == Key::Home || event.key == Key::End) {
        moveTo(event.key == Key::End ? getLength() : 0, event.shift);
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::A) {
        selectAll();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::C) {
        copy();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::X) {
        if (editable_) {
            cut();
        }
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::V) {
        if (editable_) {
            paste();
        }
        event.consume();
        return;
    }
    if (erase) {
        if (editable_) {
            eraseOne(event.key == Key::Delete);
        }
        event.consume();
        return;
    }
    if (event.key == Key::Enter || event.key == Key::KpEnter) {
        fire();
        event.consume();
    }
}

void TextField::handleText(TextEvent& event) {
    if (!editable_ || isDisabled()) {
        return;
    }
    replaceSelection(event.text);
    event.consume();
}

void TextField::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void TextField::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const float radius[4] = {4.f, 4.f, 4.f, 4.f};
    // A combo or spinner draws the box and the focus ring. A second stroke would cover them.
    const Node* parent = getParent();
    const bool hosted = parent != nullptr && (std::strcmp(parent->getElementType(), "combobox") == 0 ||
                                               std::strcmp(parent->getElementType(), "spinner") == 0);
    if (!hosted && width > 0.f && height > 0.f) {
        const ComputedStyle& style = computedStyle();
        const bool cssBorder = style.borderStyle == BorderStyle::Solid &&
                               (style.border.top > 0 || style.border.right > 0 || style.border.bottom > 0 || style.border.left > 0);
        if (!cssBorder) {
            const float sides[4] = {1.f, 1.f, 1.f, 1.f};
            Color line = Color::rgb8(218, 220, 224);
            line.a *= opacity;
            renderer.strokeRounded(x, y, width, height, radius, sides, line);
        }
        if (isFocused()) {
            const float sides[4] = {2.f, 2.f, 2.f, 2.f};
            Color ring = Color::rgb8(26, 115, 232);
            ring.a *= opacity;
            renderer.strokeRounded(x, y, width, height, radius, sides, ring);
        }
    }

    ensureCaretVisible();
    const LineLayout line = measureLine();
    const float boxX = x + static_cast<float>(contentLeft());
    const float boxY = y + static_cast<float>(contentTop());
    const float boxW = static_cast<float>(contentWidth());
    const float boxH = static_cast<float>(contentHeight());
    if (boxW <= 0.f || boxH <= 0.f || line.caretX.empty()) {
        return;
    }
    renderer.pushClip(boxX, boxY, boxW, boxH);

    const int length = static_cast<int>(line.caretX.size()) - 1;
    const int from = ClampIndex(std::min(anchor_, caret_), length);
    const int to = ClampIndex(std::max(anchor_, caret_), length);
    const float textX = boxX + line.origin;
    const float textY = boxY + line.top;
    const float square[4] = {0.f, 0.f, 0.f, 0.f};
    const float at = 0.f;
    if (from != to) {
        const float left = textX + line.caretX[static_cast<std::size_t>(from)];
        const float right = textX + line.caretX[static_cast<std::size_t>(to)];
        if (right > left) {
            Color fill = Color::rgb8(26, 115, 232);
            fill.a = 0.35f * opacity;
            renderer.fillRounded(left, textY, right - left, line.lineHeight, square, &fill, &at, 1, 0.f);
        }
    }

    const ComputedStyle& style = computedStyle();
    if (text_.empty()) {
        if (!prompt_.empty()) {
            Color color = style.color;
            color.a *= 0.5f * opacity;
            const float promptWidth = line.font.measureWidth(prompt_);
            const float promptX = boxX + AlignIn(boxW, promptWidth, hpos(usingAlignment()));
            renderer.text(promptX, textY, prompt_, style.fontFamily, style.fontSize, color, style.subpixel);
        }
    } else {
        Color color = style.color;
        color.a *= opacity;
        renderer.text(textX, textY, text_, style.fontFamily, style.fontSize, color, style.subpixel);
    }

    // Show the bar only while editing a collapsed selection. No scene means no blink clock.
    bool showCaret = editable_ && anchor_ == caret_;
    if (showCaret && getScene() != nullptr) {
        showCaret = isFocused() && std::fmod(getScene()->timeSeconds(), 1.0) < 0.5;
    }
    if (showCaret) {
        const int caret = ClampIndex(caret_, length);
        float caretX = textX + line.caretX[static_cast<std::size_t>(caret)];
        const float limit = boxX + boxW - 1.f;
        if (caretX > limit) {
            caretX = limit;
        }
        if (caretX < boxX) {
            caretX = boxX;
        }
        Color bar = style.color;
        bar.a *= opacity;
        renderer.fillRounded(caretX, textY, 1.f, line.lineHeight, square, &bar, &at, 1, 0.f);
    }
    renderer.popClip();
}

double TextField::preferredContentWidth(double) const {
    const int columns = columns_ > 0 ? columns_ : 0;
    return static_cast<double>(columns) * static_cast<double>(face().measureWidth("n"));
}

double TextField::preferredContentHeight(double) const { return face().lineHeight(); }

}  // namespace jadefx
