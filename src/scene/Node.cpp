#include "jadefx/scene/Node.hpp"

#include "jadefx/scene/Scene.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace jadefx {
namespace {

double ClampSpec(double value, const SizeSpec& minimum, const SizeSpec& maximum, double available, double fontSize) {
    if (minimum.set()) {
        value = std::max(value, resolveSize(minimum, available, fontSize));
    }
    if (maximum.set()) {
        value = std::min(value, resolveSize(maximum, available, fontSize));
    }
    return std::max(0.0, value);
}

float ResolvedRadius(const ComputedStyle& style, int index, double width, double height) {
    const double basis = std::min(width, height);
    float radius = static_cast<float>(resolveSize(style.radius[index], basis, style.fontSize));
    const float limit = static_cast<float>(basis * 0.5);
    if (radius < 0.f) {
        radius = 0.f;
    }
    if (limit > 0.f && radius > limit) {
        radius = limit;
    }
    return radius;
}

TransitionTiming TimingOf(const ComputedStyle& style, const char* property) {
    const auto found = style.transitions.find(property);
    if (found != style.transitions.end()) {
        return found->second;
    }
    const auto all = style.transitions.find("all");
    if (all != style.transitions.end()) {
        return all->second;
    }
    return {};
}

bool HasControlCursor(Cursor cursor) { return cursor != Cursor::Inherit && cursor != Cursor::Auto; }

Cursor ConcreteCursor(Cursor cursor) {
    if (cursor == Cursor::Inherit || cursor == Cursor::Auto) {
        return Cursor::Default;
    }
    return cursor;
}

Cursor ResolveCursor(Cursor specified, Cursor inheritedCursor, Cursor controlCursor, bool disabled) {
    if (specified == Cursor::Inherit) {
        return inheritedCursor;
    }
    if (specified == Cursor::Auto) {
        if (!disabled && HasControlCursor(controlCursor)) {
            return controlCursor;
        }
        return Cursor::Default;
    }
    return specified;
}

bool LastCursor(const std::vector<Declaration>& declarations, Cursor& cursor) {
    bool found = false;
    for (const Declaration& declaration : declarations) {
        if (declaration.property != "cursor") {
            continue;
        }
        Cursor parsed = Cursor::Default;
        if (parseCursor(declaration.value, parsed)) {
            cursor = parsed;
            found = true;
        }
    }
    return found;
}

Cursor ResolvedNodeCursor(const std::vector<Declaration>& declarations, Cursor inherited, Cursor controlCursor,
                          bool explicitCursor, Cursor cursor, bool disabled) {
    const Cursor inheritedCursor = ConcreteCursor(inherited);
    Cursor specified = Cursor::Inherit;
    if (LastCursor(declarations, specified)) {
        return ResolveCursor(specified, inheritedCursor, controlCursor, disabled);
    }
    if (explicitCursor) {
        return ResolveCursor(cursor, inheritedCursor, controlCursor, disabled);
    }
    if (!disabled && HasControlCursor(controlCursor)) {
        return controlCursor;
    }
    return inheritedCursor;
}

bool SameInsets(const Insets& a, const Insets& b) {
    return a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.left == b.left;
}

Insets LerpInsets(const Insets& from, const Insets& to, double t) {
    auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return {mix(from.top, to.top), mix(from.right, to.right), mix(from.bottom, to.bottom), mix(from.left, to.left)};
}

}  // namespace

Node::Node() {
    children_.setAddCallback([this](const std::shared_ptr<Node>& child) {
        if (!child) {
            return;
        }
        if (child->parent_ != nullptr && child->parent_ != this) {
            child->parent_->detachChild(child.get());
        }
        child->setParent(this);
    });
    children_.setRemoveCallback([this](const std::shared_ptr<Node>& child) {
        if (child && child->parent_ == this) {
            child->setParent(nullptr);
        }
    });
}

Node::~Node() {
    children_.setAddCallback(nullptr);
    children_.setRemoveCallback(nullptr);
    children_.clear();
}

void Node::requestFocus() {
    if (scene_ == nullptr || isDisabled()) {
        return;
    }
    scene_->requestFocus(this);
}

void Node::setCursor(Cursor cursor) {
    cursorExplicit_ = true;
    cursor_ = cursor;
}

Cursor Node::getCursor() const { return cursorExplicit_ ? cursor_ : Cursor::Inherit; }

void Node::setPseudoState(const std::string& name, bool enabled) {
    const auto found = std::find(pseudoStates_.begin(), pseudoStates_.end(), name);
    if (enabled) {
        if (found == pseudoStates_.end()) {
            pseudoStates_.push_back(name);
        }
        return;
    }
    if (found != pseudoStates_.end()) {
        pseudoStates_.erase(found);
    }
}

bool Node::pseudoState(const std::string& name) const {
    return std::find(pseudoStates_.begin(), pseudoStates_.end(), name) != pseudoStates_.end();
}

void Node::setDisable(bool value) {
    if (disable_ == value) {
        return;
    }
    disable_ = value;
    if (!value) {
        return;
    }
    pressed_ = false;
    if (scene_ != nullptr) {
        scene_->releaseFocus(this);
    }
}

bool Node::isDisabled() const {
    for (const Node* node = this; node != nullptr; node = node->parent_) {
        if (node->disable_) {
            return true;
        }
    }
    return false;
}

bool Node::isAncestorOf(const Node* node) const {
    for (const Node* cursor = node; cursor != nullptr; cursor = cursor->parent_) {
        if (cursor == this) {
            return true;
        }
    }
    return false;
}

void Node::setHoverPopup(HoverPopup popup) {
    if (!popup.content) {
        hoverPopup_.reset();
        return;
    }
    hoverPopup_ = std::move(popup);
}

void Node::clearHoverPopup() { hoverPopup_.reset(); }

const HoverPopup* Node::getHoverPopup() const {
    return hoverPopup_ ? &*hoverPopup_ : nullptr;
}

void Node::setParent(Node* parent) {
    Scene* previousScene = scene_;
    parent_ = parent;
    if (asScene() != nullptr) {
        scene_ = asScene();
    } else if (parent_ != nullptr) {
        scene_ = parent_->asScene() != nullptr ? parent_->asScene() : parent_->scene_;
    } else {
        scene_ = nullptr;
    }
    const bool sceneMoved = previousScene != scene_;
    // A scene that is already tearing down is still in its destructor. Do not
    // touch its focus or popup lists. scene_ is cleared so a later destructor
    // does not call into the freed scene.
    const bool previousAlive = previousScene != nullptr && !previousScene->isTearingDown();
    if (sceneMoved && previousAlive) {
        previousScene->releaseFocus(this);
        previousScene->hidePopupsOwnedBy(this);
    }
    if (sceneMoved) {
        // sceneChanged may see a scene that is tearing down. It must not use
        // that scene's lists; isTearingDown() is still readable.
        sceneChanged(previousScene);
    }

    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        child->setParent(this);
    }
}

double Node::getAbsoluteX() const {
    double x = x_ + translateX_;
    for (const Node* parent = parent_; parent != nullptr; parent = parent->parent_) {
        x += parent->x_ + parent->translateX_;
    }
    return x;
}

double Node::getAbsoluteY() const {
    double y = y_ + translateY_;
    for (const Node* parent = parent_; parent != nullptr; parent = parent->parent_) {
        y += parent->y_ + parent->translateY_;
    }
    return y;
}

void Node::setPrefSize(double width, double height) {
    setPrefWidth(width);
    setPrefHeight(height);
}

void Node::setPrefWidth(double width) { prefWidth_ = SizeSpec::px(width); }
void Node::setPrefHeight(double height) { prefHeight_ = SizeSpec::px(height); }
void Node::setPrefWidthRatio(double ratio) { prefWidth_ = SizeSpec::ratio(ratio); }
void Node::setPrefHeightRatio(double ratio) { prefHeight_ = SizeSpec::ratio(ratio); }

void Node::setMinSize(double width, double height) {
    minWidth_ = SizeSpec::px(width);
    minHeight_ = SizeSpec::px(height);
}

void Node::setMaxSize(double width, double height) {
    maxWidth_ = SizeSpec::px(width);
    maxHeight_ = SizeSpec::px(height);
}

double Node::getPrefWidth() const {
    return prefWidth_.kind == SizeKind::Pixels ? prefWidth_.pixels : 0;
}

double Node::getPrefHeight() const {
    return prefHeight_.kind == SizeKind::Pixels ? prefHeight_.pixels : 0;
}

double Node::getMinWidth() const {
    return minWidth_.kind == SizeKind::Pixels ? std::max(0.0, minWidth_.pixels) : 0;
}

double Node::getMinHeight() const {
    return minHeight_.kind == SizeKind::Pixels ? std::max(0.0, minHeight_.pixels) : 0;
}

Pos Node::usingAlignment() const {
    if (computed_.alignmentFromCss && computed_.alignment != Pos::Ancestor) {
        return computed_.alignment;
    }
    if (alignment_ != Pos::Ancestor) {
        return alignment_;
    }
    if (parent_ != nullptr) {
        return parent_->usingAlignment();
    }
    return Pos::Center;
}

void Node::setBackground(const Color& color) {
    background_ = color;
    backgroundExplicit_ = true;
}

void Node::setStyle(std::string css) {
    styleText_ = std::move(css);
    inline_ = parseInlineDeclarations(styleText_);
}

void Node::setStylesheet(std::string css) {
    stylesheet_ = Stylesheet::parse(css);
}

void Node::setFontInternal(const Font& font, bool explicitSize) {
    font_ = font;
    fontExplicit_ = explicitSize;
}

void Node::setTextFillInternal(const Color& color, bool explicitColor) {
    textFill_ = color;
    fillExplicit_ = explicitColor;
}

void Node::setSubpixelRenderingInternal(bool enabled) {
    subpixel_ = enabled;
    subpixelExplicit_ = true;
    computed_.subpixel = enabled;
}

double Node::contentLeft() const { return computed_.padding.left + computed_.border.left; }
double Node::contentTop() const { return computed_.padding.top + computed_.border.top; }

double Node::contentWidth() const {
    return std::max(0.0, width_ - computed_.padding.width() - computed_.border.width());
}

double Node::contentHeight() const {
    return std::max(0.0, height_ - computed_.padding.height() - computed_.border.height());
}

double Node::preferredContentWidth(double) const { return 0; }
double Node::preferredContentHeight(double) const { return 0; }
void Node::layoutChildren() {}
void Node::renderContent(UiRenderer&, float) {}

void Node::visitChildren(const std::function<void(Node*)>& visitor) {
    for (const std::shared_ptr<Node>& child : children_.items()) {
        if (child) {
            visitor(child.get());
        }
    }
}

void Node::detachChild(Node* child) {
    if (child == nullptr) {
        return;
    }
    children_.removeIf([&](const std::shared_ptr<Node>& item) { return item.get() == child; });
}

double Node::measuredWidth(double available) const {
    double width = 0;
    if (computed_.width.set()) {
        width = resolveSize(computed_.width, available, computed_.fontSize);
    } else {
        const double pad = computed_.padding.width() + computed_.border.width();
        const double inner = std::max(0.0, available - pad);
        width = preferredContentWidth(inner) + pad;
    }
    return ClampSpec(width, computed_.minWidth, computed_.maxWidth, available, computed_.fontSize);
}

double Node::measuredHeight(double width, double availableHeight) const {
    const bool percent = computed_.height.kind == SizeKind::Percent || computed_.height.kind == SizeKind::Calc;
    double height = 0;
    if (computed_.height.set() && !(percent && availableHeight < 0)) {
        const double available = availableHeight < 0 ? 0 : availableHeight;
        height = resolveSize(computed_.height, available, computed_.fontSize);
    } else {
        const double pad = computed_.padding.height() + computed_.border.height();
        const double innerWidth = std::max(0.0, width - computed_.padding.width() - computed_.border.width());
        height = preferredContentHeight(innerWidth) + pad;
    }
    const double available = availableHeight < 0 ? height : availableHeight;
    return ClampSpec(height, computed_.minHeight, computed_.maxHeight, available, computed_.fontSize);
}

void Node::performLayout(double x, double y, double width, double height) {
    x_ = x;
    y_ = y;
    width_ = std::max(0.0, width);
    height_ = std::max(0.0, height);
    layoutChildren();
}

Color Node::animateColor(ColorAnim& anim, const Color& target, double duration, double delay, double time) {
    if (!anim.ready || (duration <= 0 && delay <= 0)) {
        anim.ready = true;
        anim.displayed = target;
        anim.from = target;
        anim.to = target;
        anim.duration = 0;
        return target;
    }
    if (!near(anim.to, target)) {
        anim.from = anim.displayed;
        anim.to = target;
        anim.start = time;
        anim.duration = duration;
    }
    const double elapsed = time - anim.start - delay;
    if (elapsed <= 0) {
        anim.displayed = anim.from;
        return anim.displayed;
    }
    if (anim.duration <= 0 || elapsed >= anim.duration) {
        anim.displayed = target;
        return target;
    }
    anim.displayed = mix(anim.from, anim.to, static_cast<float>(elapsed / anim.duration));
    return anim.displayed;
}

Insets Node::animateInsets(InsetAnim& anim, const Insets& target, double duration, double delay, double time) {
    if (!anim.ready || (duration <= 0 && delay <= 0)) {
        anim.ready = true;
        anim.displayed = target;
        anim.from = target;
        anim.to = target;
        anim.duration = 0;
        return target;
    }
    if (!SameInsets(anim.to, target)) {
        anim.from = anim.displayed;
        anim.to = target;
        anim.start = time;
        anim.duration = duration;
    }
    const double elapsed = time - anim.start - delay;
    if (elapsed <= 0) {
        anim.displayed = anim.from;
        return anim.displayed;
    }
    if (anim.duration <= 0 || elapsed >= anim.duration) {
        anim.displayed = target;
        return target;
    }
    anim.displayed = LerpInsets(anim.from, anim.to, elapsed / anim.duration);
    return anim.displayed;
}

void Node::applyStyles(const ComputedStyle& inherited, double timeSeconds) {
    ComputedStyle style;
    style.color = inherited.color;
    style.fontSize = inherited.fontSize > 0.f ? inherited.fontSize : 16.f;
    style.fontFamily = inherited.fontFamily.empty() ? "Open Sans" : inherited.fontFamily;
    style.subpixel = inherited.subpixel;
    style.alignment = alignment_;
    style.padding = padding_;
    style.border = border_;
    style.spacing = spacing_;
    style.opacity = opacity_;
    style.width = prefWidth_;
    style.height = prefHeight_;
    style.minWidth = minWidth_;
    style.minHeight = minHeight_;
    style.maxWidth = maxWidth_;
    style.maxHeight = maxHeight_;
    if (backgroundExplicit_) {
        style.background.color = background_;
        style.background.hasColor = true;
        style.background.gradient = false;
        style.background.stopCount = 0;
        style.background.visible = background_.a > 0.f;
    }
    if (fontExplicit_) {
        style.fontSize = font_.size();
        style.fontFamily = font_.family();
    }
    if (fillExplicit_) {
        style.color = textFill_;
    }
    if (subpixelExplicit_) {
        style.subpixel = subpixel_;
    }

    std::vector<const Node*> chain;
    for (const Node* node = this; node != nullptr; node = node->parent_) {
        chain.push_back(node);
    }
    std::vector<Declaration> matched;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (!(*it)->stylesheet_.empty()) {
            (*it)->stylesheet_.collectMatching(*this, matched);
        }
    }
    matched.insert(matched.end(), inline_.begin(), inline_.end());
    const float inheritedFont = inherited.fontSize > 0.f ? inherited.fontSize : 16.f;
    applyDeclarations(style, matched, StylePass::Fonts, inheritedFont, inheritedFont);
    applyDeclarations(style, matched, StylePass::Rest, inheritedFont, style.fontSize);
    style.cursor = ResolvedNodeCursor(matched, inherited.cursor, defaultCursor_, cursorExplicit_, cursor_, isDisabled());

    const ComputedStyle target = style;
    const TransitionTiming backgroundTiming = TimingOf(target, "background-color");
    const TransitionTiming imageTiming = TimingOf(target, "background-image");
    const TransitionTiming colorTiming = TimingOf(target, "color");
    const TransitionTiming borderColorTiming = TimingOf(target, "border-color");
    const TransitionTiming borderTiming = TimingOf(target, "border-width");
    const TransitionTiming shadowTiming = TimingOf(target, "box-shadow");
    style.background.color = animateColor(backgroundAnim_, target.background.color, backgroundTiming.duration,
                                          backgroundTiming.delay, timeSeconds);
    const int stops = std::min(target.background.stopCount, kMaxGradientStops);
    for (int i = 0; i < stops; ++i) {
        style.background.stops[i] = animateColor(stopAnim_[i], target.background.stops[i], imageTiming.duration,
                                                 imageTiming.delay, timeSeconds);
    }
    style.color = animateColor(colorAnim_, target.color, colorTiming.duration, colorTiming.delay, timeSeconds);
    style.borderColor = animateColor(borderColorAnim_, target.borderColor, borderColorTiming.duration,
                                     borderColorTiming.delay, timeSeconds);
    style.border = animateInsets(borderAnim_, target.border, borderTiming.duration, borderTiming.delay, timeSeconds);
    style.shadows = target.shadows;
    if (!shadowAnim_.ready || (shadowTiming.duration <= 0 && shadowTiming.delay <= 0) ||
        shadowAnim_.to.size() != target.shadows.size()) {
        shadowAnim_.ready = true;
        shadowAnim_.displayed = target.shadows;
        shadowAnim_.from = target.shadows;
        shadowAnim_.to = target.shadows;
        shadowAnim_.duration = 0;
    } else {
        bool changed = shadowAnim_.to.size() != target.shadows.size();
        for (std::size_t i = 0; !changed && i < target.shadows.size(); ++i) {
            const BoxShadow& from = shadowAnim_.to[i];
            const BoxShadow& to = target.shadows[i];
            changed = from.inset != to.inset || from.offsetX != to.offsetX || from.offsetY != to.offsetY ||
                      from.blur != to.blur || from.spread != to.spread || !near(from.color, to.color);
        }
        if (changed) {
            shadowAnim_.from = shadowAnim_.displayed;
            shadowAnim_.to = target.shadows;
            shadowAnim_.start = timeSeconds;
            shadowAnim_.duration = shadowTiming.duration;
        }
        const double elapsed = timeSeconds - shadowAnim_.start - shadowTiming.delay;
        if (shadowAnim_.from.size() == shadowAnim_.to.size() && elapsed > 0 && shadowAnim_.duration > 0 &&
            elapsed < shadowAnim_.duration) {
            const double t = elapsed / shadowAnim_.duration;
            shadowAnim_.displayed.resize(shadowAnim_.to.size());
            for (std::size_t i = 0; i < shadowAnim_.to.size(); ++i) {
                const BoxShadow& from = shadowAnim_.from[i];
                const BoxShadow& to = shadowAnim_.to[i];
                BoxShadow blended = to;
                auto mixNumber = [t](double a, double b) { return a + (b - a) * t; };
                blended.offsetX = mixNumber(from.offsetX, to.offsetX);
                blended.offsetY = mixNumber(from.offsetY, to.offsetY);
                blended.blur = mixNumber(from.blur, to.blur);
                blended.spread = mixNumber(from.spread, to.spread);
                blended.color = mix(from.color, to.color, static_cast<float>(t));
                shadowAnim_.displayed[i] = blended;
            }
            style.shadows = shadowAnim_.displayed;
        } else if (elapsed <= 0) {
            style.shadows = shadowAnim_.from;
        }
    }

    computed_ = style;
    styleDidApply();
    ComputedStyle pass;
    pass.color = computed_.color;
    pass.fontSize = computed_.fontSize;
    pass.fontFamily = computed_.fontFamily;
    pass.subpixel = computed_.subpixel;
    pass.cursor = computed_.cursor;
    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        child->applyStyles(pass, timeSeconds);
    }
}

bool Node::isFocusWithin() {
    if (focused_) {
        return true;
    }
    bool found = false;
    visitChildren([&](Node* child) {
        if (!found && child->isFocusWithin()) {
            found = true;
        }
    });
    return found;
}

bool Node::contains(double x, double y) const {
    const double left = getAbsoluteX();
    const double top = getAbsoluteY();
    return x >= left && y >= top && x < left + width_ && y < top + height_;
}

Node* Node::pick(double x, double y) {
    if (!visible_ || mouseTransparent_ || isDisabled() || !contains(x, y)) {
        return nullptr;
    }
    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (std::size_t i = kids.size(); i > 0; --i) {
        if (Node* hit = kids[i - 1]->pick(x, y)) {
            return hit;
        }
    }
    return this;
}

Node* Node::pickCursorTarget(double x, double y) {
    if (!visible_ || mouseTransparent_ || !contains(x, y)) {
        return nullptr;
    }
    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (std::size_t i = kids.size(); i > 0; --i) {
        if (Node* hit = kids[i - 1]->pickCursorTarget(x, y)) {
            return hit;
        }
    }
    return this;
}

Cursor Node::cursorAt(double, double) const {
    return ConcreteCursor(computed_.cursor);
}

Node* Node::getElementById(const std::string& id) {
    if (id_ == id) {
        return this;
    }
    Node* found = nullptr;
    visitChildren([&](Node* child) {
        if (found == nullptr) {
            found = child->getElementById(id);
        }
    });
    return found;
}

std::vector<Node*> Node::getElementsByClassName(const std::string& className) {
    std::vector<Node*> found;
    bool mine = false;
    for (const std::string& name : classList_.items()) {
        if (name == className) {
            mine = true;
        }
    }
    if (mine) {
        found.push_back(this);
    }
    visitChildren([&](Node* child) {
        const std::vector<Node*> nested = child->getElementsByClassName(className);
        found.insert(found.end(), nested.begin(), nested.end());
    });
    return found;
}

void Node::syncHover(Node* hit) {
    std::vector<Node*> all;
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        all.push_back(node);
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
    for (Node* node : all) {
        node->wasHovered_ = node->hovered_;
        node->hovered_ = false;
    }
    for (Node* node = hit; node != nullptr; node = node->parent_) {
        node->hovered_ = true;
    }
    for (Node* node : all) {
        if (node->wasHovered_ == node->hovered_) {
            continue;
        }
        MouseEvent event;
        event.target = node;
        event.x = node->getAbsoluteX();
        event.y = node->getAbsoluteY();
        if (node->hovered_ && node->onEntered_) {
            node->onEntered_(event);
        }
        if (!node->hovered_ && node->onExited_) {
            node->onExited_(event);
        }
    }
}

void Node::setPressedChain(Node* hit) {
    std::vector<Node*> all;
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        all.push_back(node);
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
    for (Node* node : all) {
        node->pressed_ = false;
    }
    for (Node* node = hit; node != nullptr; node = node->parent_) {
        node->pressed_ = true;
    }
}

void Node::clearFocus() {
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        node->focused_ = false;
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
}

void Node::markFocused(Node* hit) {
    if (hit != nullptr) {
        hit->focused_ = true;
    }
}

void Node::fireMouse(const MouseHandler Node::* handler, const MouseEvent& event) {
    for (Node* node = event.target; node != nullptr; node = node->parent_) {
        const MouseHandler& callback = node->*handler;
        if (callback) {
            MouseEvent delivered = event;
            delivered.target = node;
            callback(delivered);
        }
    }
}

void Node::drawChrome(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    if (w <= 0.f || h <= 0.f) {
        return;
    }
    float radius[4];
    for (int i = 0; i < 4; ++i) {
        radius[i] = ResolvedRadius(computed_, i, width_, height_);
    }
    for (const BoxShadow& shadow : computed_.shadows) {
        if (shadow.inset || shadow.color.a <= 0.f) {
            continue;
        }
        Color color = shadow.color;
        color.a *= opacity;
        renderer.outerShadow(x, y, w, h, radius, static_cast<float>(shadow.offsetX), static_cast<float>(shadow.offsetY),
                             static_cast<float>(shadow.blur), static_cast<float>(shadow.spread), color);
    }

    const float borderTop = static_cast<float>(computed_.border.top);
    const float borderRight = static_cast<float>(computed_.border.right);
    const float borderBottom = static_cast<float>(computed_.border.bottom);
    const float borderLeft = static_cast<float>(computed_.border.left);
    const float sides[4] = {borderTop, borderRight, borderBottom, borderLeft};
    const bool hasBorder = computed_.borderStyle == BorderStyle::Solid && computed_.borderColor.a > 0.f &&
                           (borderTop > 0.f || borderRight > 0.f || borderBottom > 0.f || borderLeft > 0.f);
    if (hasBorder) {
        Color color = computed_.borderColor;
        color.a *= opacity;
        renderer.strokeRounded(x, y, w, h, radius, sides, color);
    }

    if (computed_.background.visible) {
        float innerRadius[4] = {
            std::max(0.f, radius[0] - std::max(borderTop, borderLeft)),
            std::max(0.f, radius[1] - std::max(borderTop, borderRight)),
            std::max(0.f, radius[2] - std::max(borderBottom, borderRight)),
            std::max(0.f, radius[3] - std::max(borderBottom, borderLeft)),
        };
        const float fillX = x + borderLeft;
        const float fillY = y + borderTop;
        const float fillW = std::max(0.f, w - borderLeft - borderRight);
        const float fillH = std::max(0.f, h - borderTop - borderBottom);
        if (computed_.background.hasColor && computed_.background.color.a > 0.f) {
            Color color = computed_.background.color;
            color.a *= opacity;
            const float at = 0.f;
            renderer.fillRounded(fillX, fillY, fillW, fillH, innerRadius, &color, &at, 1, 0.f);
        }
        if (computed_.background.gradient && computed_.background.stopCount >= 2) {
            Color stops[kMaxGradientStops];
            float at[kMaxGradientStops] = {};
            const int count = std::min(computed_.background.stopCount, kMaxGradientStops);
            for (int i = 0; i < count; ++i) {
                stops[i] = computed_.background.stops[i];
                stops[i].a *= opacity;
                at[i] = computed_.background.stopAt[i];
            }
            renderer.fillRounded(fillX, fillY, fillW, fillH, innerRadius, stops, at, count,
                                 computed_.background.angleDeg);
        }
    }

    for (const BoxShadow& shadow : computed_.shadows) {
        if (!shadow.inset || shadow.color.a <= 0.f) {
            continue;
        }
        Color color = shadow.color;
        color.a *= opacity;
        renderer.innerShadow(x, y, w, h, radius, static_cast<float>(shadow.offsetX), static_cast<float>(shadow.offsetY),
                             static_cast<float>(shadow.blur), static_cast<float>(shadow.spread), color);
    }
}

void Node::renderChildren(UiRenderer& renderer, float opacity) {
    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        child->render(renderer, opacity);
    }
}

void Node::render(UiRenderer& renderer, float opacity) {
    if (!visible_) {
        return;
    }
    const float next = opacity * computed_.opacity;
    drawChrome(renderer, next);
    renderChildren(renderer, next);
    renderContent(renderer, next);
}

}  // namespace jadefx
