#include "jadefx/scene/Node.hpp"

#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/SubScene.hpp"
#include "jadefx/scene/image/Image.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string_view>
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

TransitionTiming TimingOf(const ComputedStyle& style, PropertyId property) {
    if (!style.transitions) {
        return {};
    }
    const TransitionTable& table = *style.transitions;
    const auto at = static_cast<std::size_t>(property);
    if (table.set[at]) {
        return table.timing[at];
    }
    const auto all = static_cast<std::size_t>(PropertyId::All);
    return table.set[all] ? table.timing[all] : TransitionTiming{};
}

// The lists one restyle or one child walk needs. Kept between frames so a pass
// allocates nothing once the lists have grown.
struct StyleScratch {
    std::vector<MatchedDeclaration> agentMatches;
    std::vector<MatchedDeclaration> authorMatches;
    std::vector<const Node*> chain;
    std::vector<const Declaration*> agent;
    std::vector<const Declaration*> author;
    std::vector<const Declaration*> agentImportant;
    std::vector<const Declaration*> authorImportant;
    std::vector<const Declaration*> inlineImportant;
    std::vector<const Declaration*> variables;
    std::vector<Node*> kids;

    void clear() {
        agentMatches.clear();
        authorMatches.clear();
        chain.clear();
        agent.clear();
        author.clear();
        agentImportant.clear();
        authorImportant.clear();
        inlineImportant.clear();
        variables.clear();
        kids.clear();
    }
};

// One StyleScratch per nesting level. A restyle holds one while its children
// restyle, and a control may call applyCss from styleDidApply, so leases nest.
class ScratchLease {
public:
    ScratchLease() {
        Pool& pool = ThePool();
        if (pool.used == pool.items.size()) {
            pool.items.push_back(std::make_unique<StyleScratch>());
        }
        scratch_ = pool.items[pool.used++].get();
        scratch_->clear();
    }
    ~ScratchLease() { --ThePool().used; }
    ScratchLease(const ScratchLease&) = delete;
    ScratchLease& operator=(const ScratchLease&) = delete;

    StyleScratch& operator*() { return *scratch_; }

private:
    struct Pool {
        std::vector<std::unique_ptr<StyleScratch>> items;
        std::size_t used = 0;
    };
    static Pool& ThePool() {
        thread_local Pool pool;
        return pool;
    }
    StyleScratch* scratch_ = nullptr;
};

thread_local int gLayoutPassDepth = 0;
thread_local std::uint64_t gLayoutPassEpoch = 0;
thread_local bool gFullPass = false;

bool SameColor(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool SameVariables(const std::shared_ptr<const CssVariables>& a, const std::shared_ptr<const CssVariables>& b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return (!a || a->empty()) && (!b || b->empty());
    }
    return *a == *b;
}

// What inheritableStyle passes down: text color, font, smoothing, cursor, and custom properties.
bool SameInheritable(const ComputedStyle& a, const ComputedStyle& b) {
    return SameColor(a.color, b.color) && a.fontSize == b.fontSize && a.fontFamily == b.fontFamily &&
           a.subpixel == b.subpixel && a.cursor == b.cursor && SameVariables(a.variables, b.variables);
}

// Theme colors parsed from one set of custom properties. Nodes share sets, so a
// few entries serve the whole tree. Each holds its set alive, so its address
// cannot be reused by another set while the entry exists.
struct ThemeCacheEntry {
    std::shared_ptr<const CssVariables> variables;
    // 0 not looked up yet, 1 a color, 2 currentColor.
    unsigned char state[kThemeColorCount] = {};
    Color colors[kThemeColorCount] = {};
};

constexpr std::size_t kThemeCacheEntries = 8;
thread_local std::vector<ThemeCacheEntry> gThemeCache;

ThemeCacheEntry& ThemeCacheFor(const std::shared_ptr<const CssVariables>& variables) {
    for (ThemeCacheEntry& entry : gThemeCache) {
        if (entry.variables == variables) {
            return entry;
        }
    }
    if (gThemeCache.size() >= kThemeCacheEntries) {
        gThemeCache.clear();
    }
    gThemeCache.emplace_back();
    gThemeCache.back().variables = variables;
    return gThemeCache.back();
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

bool LastCursor(const std::vector<const Declaration*>& declarations, Cursor& cursor) {
    bool found = false;
    for (const Declaration* declaration : declarations) {
        if (declaration->id != PropertyId::Cursor) {
            continue;
        }
        Cursor parsed = Cursor::Default;
        if (parseCursor(declaration->value, parsed)) {
            cursor = parsed;
            found = true;
        }
    }
    return found;
}

// agent is the user-agent layer and author everything above code-set values.
Cursor ResolvedNodeCursor(const std::vector<const Declaration*>& agent, const std::vector<const Declaration*>& author,
                          Cursor inherited, Cursor controlCursor, bool explicitCursor, Cursor cursor, bool disabled) {
    const Cursor inheritedCursor = ConcreteCursor(inherited);
    Cursor specified = Cursor::Inherit;
    if (LastCursor(author, specified)) {
        return ResolveCursor(specified, inheritedCursor, controlCursor, disabled);
    }
    if (explicitCursor) {
        return ResolveCursor(cursor, inheritedCursor, controlCursor, disabled);
    }
    if (LastCursor(agent, specified)) {
        return ResolveCursor(specified, inheritedCursor, controlCursor, disabled);
    }
    if (!disabled && HasControlCursor(controlCursor)) {
        return controlCursor;
    }
    return inheritedCursor;
}

bool SameInsets(const Insets& a, const Insets& b) {
    return a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.left == b.left;
}

bool SameSize(const SizeSpec& a, const SizeSpec& b) {
    return a.kind == b.kind && a.pixels == b.pixels && a.percent == b.percent && a.em == b.em;
}

// Fields that change a node's preferred size or how it places its children.
// Colors, shadows, radii, opacity, and the cursor only change how it paints.
bool LayoutAffectingChange(const ComputedStyle& a, const ComputedStyle& b) {
    return !SameInsets(a.padding, b.padding) || !SameInsets(a.border, b.border) || a.fontSize != b.fontSize ||
           a.fontFamily != b.fontFamily || a.subpixel != b.subpixel || !SameSize(a.width, b.width) ||
           !SameSize(a.height, b.height) || !SameSize(a.minWidth, b.minWidth) || !SameSize(a.minHeight, b.minHeight) ||
           !SameSize(a.maxWidth, b.maxWidth) || !SameSize(a.maxHeight, b.maxHeight) || a.spacing != b.spacing ||
           a.rowGap != b.rowGap || a.columnGap != b.columnGap || a.alignment != b.alignment ||
           a.alignmentFromCss != b.alignmentFromCss || a.orientationFromCss != b.orientationFromCss ||
           a.orientation != b.orientation || a.indeterminateBarLengthSet != b.indeterminateBarLengthSet ||
           !SameSize(a.indeterminateBarLength, b.indeterminateBarLength) ||
           a.indeterminateBarEscapeSet != b.indeterminateBarEscapeSet ||
           a.indeterminateBarEscape != b.indeterminateBarEscape ||
           a.indeterminateBarFlipSet != b.indeterminateBarFlipSet || a.indeterminateBarFlip != b.indeterminateBarFlip ||
           a.indeterminateBarAnimationTimeSet != b.indeterminateBarAnimationTimeSet ||
           a.indeterminateBarAnimationTime != b.indeterminateBarAnimationTime;
}

Insets LerpInsets(const Insets& from, const Insets& to, double t) {
    auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return {mix(from.top, to.top), mix(from.right, to.right), mix(from.bottom, to.bottom), mix(from.left, to.left)};
}

}  // namespace

namespace detail {
void clearThemeColorCache() { gThemeCache.clear(); }
}  // namespace detail

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
    // A class can appear in any compound of a descendant selector.
    classList_.addListener([this](const ObservableList<std::string>::Change&) { markStyleDirty(StyleDirt::Subtree); });
}

void Node::setAlignment(Pos pos) {
    if (alignment_ == pos) {
        return;
    }
    alignment_ = pos;
    markStyleDirty();
}

void Node::setPadding(const Insets& insets) {
    if (SameInsets(padding_, insets)) {
        return;
    }
    padding_ = insets;
    markStyleDirty();
}

void Node::setBorder(const Insets& insets) {
    if (SameInsets(border_, insets)) {
        return;
    }
    border_ = insets;
    markStyleDirty();
}

void Node::setOpacity(float opacity) {
    if (opacity_ == opacity) {
        return;
    }
    opacity_ = opacity;
    markStyleDirty();
}

void Node::setPressed(bool pressed) {
    if (pressed_ == pressed) {
        return;
    }
    pressed_ = pressed;
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setSelected(bool selected) {
    if (selected_ == selected) {
        return;
    }
    selected_ = selected;
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setElementId(std::string id) {
    if (id_ == id) {
        return;
    }
    id_ = std::move(id);
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setSpacingValue(double spacing) {
    const float value = static_cast<float>(spacing);
    if (spacing_ == value) {
        return;
    }
    spacing_ = value;
    markStyleDirty();
}

void Node::setDefaultCursor(Cursor cursor) {
    if (defaultCursor_ == cursor) {
        return;
    }
    defaultCursor_ = cursor;
    markStyleDirty();
}

Node::~Node() {
    // A hover popup the scene or the application still holds outlives this node.
    if (hoverPopup_ && hoverPopup_->content) {
        hoverPopup_->content->hoverHostReleased(this);
    }
    children_.setAddCallback(nullptr);
    children_.setRemoveCallback(nullptr);
    // A child can outlive this node, such as a menu item's graphic shared by
    // each rebuilt row. Drop its back pointer so a later add does not detach
    // it from freed memory.
    for (const std::shared_ptr<Node>& child : children_.items()) {
        if (child && child->parent_ == this) {
            child->parent_ = nullptr;
        }
    }
    children_.clear();
}

void Node::requestFocus() {
    if (scene_ == nullptr || isDisabled()) {
        return;
    }
    scene_->requestFocus(this);
}

void Node::setVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    // Containers may skip hidden children when they measure and place them.
    markLayoutDirty();
}

void Node::setCursor(Cursor cursor) {
    if (cursorExplicit_ && cursor_ == cursor) {
        return;
    }
    cursorExplicit_ = true;
    cursor_ = cursor;
    markStyleDirty();
}

Cursor Node::getCursor() const { return cursorExplicit_ ? cursor_ : Cursor::Inherit; }

void Node::setPseudoState(const std::string& name, bool enabled) {
    const auto found = std::find(pseudoStates_.begin(), pseudoStates_.end(), name);
    if (enabled) {
        if (found == pseudoStates_.end()) {
            pseudoStates_.push_back(name);
            markStyleDirty(StyleDirt::Subtree);
        }
        return;
    }
    if (found != pseudoStates_.end()) {
        pseudoStates_.erase(found);
        markStyleDirty(StyleDirt::Subtree);
    }
}

bool Node::pseudoState(const std::string& name) const {
    return std::find(pseudoStates_.begin(), pseudoStates_.end(), name) != pseudoStates_.end();
}

int Node::getNthChildIndex() const {
    if (parent_ == nullptr) {
        return 0;
    }
    int position = 0;
    int found = 0;
    parent_->visitChildren([&](Node* child) {
        ++position;
        if (child == this) {
            found = position;
        }
    });
    return found;
}

void Node::setDisable(bool value) {
    if (disable_ == value) {
        return;
    }
    disable_ = value;
    // A descendant's :disabled and its cursor follow an ancestor's flag.
    markStyleDirty(StyleDirt::Subtree);
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
        clearHoverPopup();
        return;
    }
    // Republishing the same content, as new delays do, keeps it on this host.
    std::shared_ptr<Node> previous = hoverPopup_ ? hoverPopup_->content : nullptr;
    hoverPopup_ = std::move(popup);
    if (previous && previous != hoverPopup_->content) {
        previous->hoverHostReleased(this);
    }
}

void Node::clearHoverPopup() {
    // Held until the host has let go, so the notice reaches a live popup.
    std::shared_ptr<Node> previous = hoverPopup_ ? hoverPopup_->content : nullptr;
    hoverPopup_.reset();
    if (previous) {
        previous->hoverHostReleased(this);
    }
}

const HoverPopup* Node::getHoverPopup() const {
    return hoverPopup_ ? &*hoverPopup_ : nullptr;
}

void Node::setParent(Node* parent) {
    Node* const previousParent = parent_;
    Scene* previousScene = scene_;
    // A subtree holding the focus takes its count from the old ancestors to the new.
    const int focusCount = focusWithinCount_;
    if (focusCount > 0 && previousParent != parent && previousParent != nullptr) {
        previousParent->addFocusWithin(-focusCount);
    }
    parent_ = parent;
    if (focusCount > 0 && previousParent != parent && parent_ != nullptr) {
        parent_->addFocusWithin(focusCount);
    }
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
        previousScene->forgetNode(this);
    }
    if (sceneMoved) {
        // sceneChanged may see a scene that is tearing down. It must not use
        // that scene's lists; isTearingDown() is still readable.
        sceneChanged(previousScene);
    }
    if (previousParent != parent_) {
        // A node that moves is styled and placed again where it lands, and the lists
        // it left and joined change order.
        if (previousParent != nullptr) {
            previousParent->childrenChanged();
        }
        if (parent_ != nullptr) {
            parent_->childrenChanged();
        }
        markStyleDirty(StyleDirt::Subtree);
        markSubtreeLayoutDirty();
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

void Node::setSizeSpec(SizeSpec& slot, const SizeSpec& value) {
    if (SameSize(slot, value)) {
        return;
    }
    slot = value;
    markStyleDirty();
}

void Node::setPrefWidth(double width) { setSizeSpec(prefWidth_, SizeSpec::px(width)); }
void Node::setPrefHeight(double height) { setSizeSpec(prefHeight_, SizeSpec::px(height)); }
void Node::setPrefWidthRatio(double ratio) { setSizeSpec(prefWidth_, SizeSpec::ratio(ratio)); }
void Node::setPrefHeightRatio(double ratio) { setSizeSpec(prefHeight_, SizeSpec::ratio(ratio)); }

void Node::setMinSize(double width, double height) {
    setSizeSpec(minWidth_, SizeSpec::px(width));
    setSizeSpec(minHeight_, SizeSpec::px(height));
}

void Node::setMaxSize(double width, double height) {
    setSizeSpec(maxWidth_, SizeSpec::px(width));
    setSizeSpec(maxHeight_, SizeSpec::px(height));
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
    if (backgroundExplicit_ && SameColor(background_, color)) {
        return;
    }
    background_ = color;
    backgroundExplicit_ = true;
    markStyleDirty();
}

void Node::setBackgroundImage(std::shared_ptr<Image> image, float opacity) {
    backgroundImage_ = std::move(image);
    backgroundImageOpacity_ = std::clamp(opacity, 0.f, 1.f);
}

void Node::setStyle(std::string css) {
    if (styleText_ == css) {
        return;
    }
    styleText_ = std::move(css);
    inline_ = parseInlineDeclarations(styleText_);
    markStyleDirty();
}

void Node::setStylesheet(std::string css) {
    stylesheet_ = Stylesheet::parse(css);
    // A sheet covers its subtree.
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setFontInternal(const Font& font, bool explicitSize) {
    if (fontExplicit_ == explicitSize && font_.family() == font.family() && font_.size() == font.size()) {
        return;
    }
    font_ = font;
    fontExplicit_ = explicitSize;
    markStyleDirty();
}

void Node::setTextFillInternal(const Color& color, bool explicitColor) {
    if (fillExplicit_ == explicitColor && SameColor(textFill_, color)) {
        return;
    }
    textFill_ = color;
    fillExplicit_ = explicitColor;
    markStyleDirty();
}

void Node::setSubpixelRenderingInternal(bool enabled) {
    if (subpixelExplicit_ && subpixel_ == enabled) {
        return;
    }
    markStyleDirty();
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

void Node::beginLayoutPass() {
    if (gLayoutPassDepth++ == 0) {
        ++gLayoutPassEpoch;
    }
}

void Node::endLayoutPass() { --gLayoutPassDepth; }

bool Node::measureCacheUsable() const {
    if (gLayoutPassDepth == 0) {
        return false;
    }
    // An incremental pass keeps measures until a mark clears them. A full pass
    // starts each pass fresh, as every node is styled again.
    if (!incrementalActive() && measure_.epoch != gLayoutPassEpoch) {
        measure_.clear();
        measure_.epoch = gLayoutPassEpoch;
    }
    return true;
}

void Node::setFullPass(bool full) { gFullPass = full; }

bool Node::incrementalActive() const { return !gFullPass && scene_ != nullptr && scene_->incremental_; }

void Node::markStyleDirty(StyleDirt dirt) {
    styleDirty_ = true;
    if (dirt == StyleDirt::Subtree) {
        styleSubtreeDirty_ = true;
    }
    for (Node* node = parent_; node != nullptr && !node->childStyleDirty_; node = node->parent_) {
        node->childStyleDirty_ = true;
    }
}

void Node::markLayoutDirty(LayoutDirt dirt) {
    layoutDirty_ = true;
    if (dirt == LayoutDirt::Arrange) {
        for (Node* node = parent_; node != nullptr && !node->childLayoutDirty_ && !node->layoutDirty_;
             node = node->parent_) {
            node->childLayoutDirty_ = true;
        }
        return;
    }
    // A preferred size feeds every ancestor's, so the whole chain measures and lays
    // out again. The walk always reaches the root: it is short, and stopping early
    // could leave an ancestor's measure cached from earlier in this pass.
    measure_.clear();
    for (Node* node = parent_; node != nullptr; node = node->parent_) {
        node->measure_.clear();
        node->layoutDirty_ = true;
    }
}

void Node::markSubtreeLayoutDirty() {
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        node->layoutDirty_ = true;
        node->measure_.clear();
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
    markLayoutDirty(LayoutDirt::Size);
}

void Node::childrenChanged() {
    if (tearingDown_) {
        return;
    }
    visitChildren([](Node* child) { child->markStyleDirty(StyleDirt::Subtree); });
    markLayoutDirty(LayoutDirt::Size);
}

double Node::measuredWidth(double available) const {
    const bool cached = measureCacheUsable();
    if (cached) {
        for (int i = 0; i < measure_.widthCount; ++i) {
            if (measure_.widths[i].available == available) {
                return measure_.widths[i].result;
            }
        }
    }
    double width = 0;
    if (computed_.width.set()) {
        width = resolveSize(computed_.width, available, computed_.fontSize);
    } else {
        const double pad = computed_.padding.width() + computed_.border.width();
        const double inner = std::max(0.0, available - pad);
        width = preferredContentWidth(inner) + pad;
    }
    const double result = ClampSpec(width, computed_.minWidth, computed_.maxWidth, available, computed_.fontSize);
    if (cached) {
        measure_.widths[measure_.widthNext] = {available, result};
        measure_.widthNext = (measure_.widthNext + 1) % 2;
        measure_.widthCount = std::min(measure_.widthCount + 1, 2);
    }
    return result;
}

double Node::measuredHeight(double width, double availableHeight) const {
    const bool cached = measureCacheUsable();
    if (cached) {
        for (int i = 0; i < measure_.heightCount; ++i) {
            const MeasureCache::Height& entry = measure_.heights[i];
            if (entry.width == width && entry.available == availableHeight) {
                return entry.result;
            }
        }
    }
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
    const double result = ClampSpec(height, computed_.minHeight, computed_.maxHeight, available, computed_.fontSize);
    if (cached) {
        measure_.heights[measure_.heightNext] = {width, availableHeight, result};
        measure_.heightNext = (measure_.heightNext + 1) % 2;
        measure_.heightCount = std::min(measure_.heightCount + 1, 2);
    }
    return result;
}

void Node::layoutChildrenAndForgetMeasures() {
    layoutChildren();
    // Laying out can change what a node measures, as for a page that learns its
    // height by placing its rows. Its own measures and its ancestors' are fresh after.
    for (const Node* node = this; node != nullptr; node = node->parent_) {
        node->measure_.clear();
    }
}

void Node::performLayout(double x, double y, double width, double height) {
    width = std::max(0.0, width);
    height = std::max(0.0, height);
    const bool resized = !hasBounds_ || width != width_ || height != height_;
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;
    hasBounds_ = true;
    if (!incrementalActive()) {
        layoutDirty_ = false;
        childLayoutDirty_ = false;
        layoutChildrenAndForgetMeasures();
        return;
    }
    // A clean node keeps its children where they are. Its position is relative to its
    // parent, so moving it moves them too.
    if (!resized && !layoutDirty_ && !childLayoutDirty_) {
        return;
    }
    const bool whole = resized || layoutDirty_;
    layoutDirty_ = false;
    childLayoutDirty_ = false;
    if (whole) {
        ++layoutCount_;
        layoutChildrenAndForgetMeasures();
        return;
    }
    // Only children below need it: lay each dirty one out again where it is.
    ScratchLease lease;
    std::vector<Node*>& kids = (*lease).kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        if (child->layoutDirty_ || child->childLayoutDirty_) {
            child->performLayout(child->x_, child->y_, child->width_, child->height_);
        }
    }
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

bool Node::resolveStyle(const ComputedStyle& inherited, double timeSeconds) {
    ScratchLease lease;
    StyleScratch& scratch = *lease;
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
    style.variables = inherited.variables;

    // The cascade, lowest first, as in CSS and JavaFX: the user-agent stylesheet,
    // then values set from code, then author stylesheets from the root down,
    // then inline declarations. Within a stylesheet the more specific selector
    // wins, and among equals the later one. !important declarations come after
    // every normal one: author, then inline, then user agent.
    if (const Stylesheet* agentSheet = userAgentSheet()) {
        agentSheet->collectMatching(*this, scratch.agentMatches);
    }
    for (const Node* node = this; node != nullptr; node = node->parent_) {
        scratch.chain.push_back(node);
        // A SubScene's root is the top of its cascade.
        if (node->parent_ != nullptr && node->parent_->asSubScene() != nullptr) {
            break;
        }
    }
    for (auto it = scratch.chain.rbegin(); it != scratch.chain.rend(); ++it) {
        if (!(*it)->stylesheet_.empty()) {
            (*it)->stylesheet_.collectMatching(*this, scratch.authorMatches);
        }
    }
    const auto bySpecificity = [](const MatchedDeclaration& a, const MatchedDeclaration& b) {
        return a.specificity < b.specificity;
    };
    std::stable_sort(scratch.agentMatches.begin(), scratch.agentMatches.end(), bySpecificity);
    std::stable_sort(scratch.authorMatches.begin(), scratch.authorMatches.end(), bySpecificity);
    for (const MatchedDeclaration& match : scratch.agentMatches) {
        (match.declaration->important ? scratch.agentImportant : scratch.agent).push_back(match.declaration);
    }
    for (const MatchedDeclaration& match : scratch.authorMatches) {
        (match.declaration->important ? scratch.authorImportant : scratch.author).push_back(match.declaration);
    }
    for (const Declaration& declaration : inline_) {
        (declaration.important ? scratch.inlineImportant : scratch.author).push_back(&declaration);
    }
    scratch.author.insert(scratch.author.end(), scratch.authorImportant.begin(), scratch.authorImportant.end());
    scratch.author.insert(scratch.author.end(), scratch.inlineImportant.begin(), scratch.inlineImportant.end());
    scratch.author.insert(scratch.author.end(), scratch.agentImportant.begin(), scratch.agentImportant.end());

    const float inheritedFont = inherited.fontSize > 0.f ? inherited.fontSize : 16.f;
    // Custom properties take one pass over every origin, so a var() in one is
    // resolved against the values the whole cascade leaves on this node.
    scratch.variables.assign(scratch.agent.begin(), scratch.agent.end());
    scratch.variables.insert(scratch.variables.end(), scratch.author.begin(), scratch.author.end());
    applyDeclarations(style, scratch.variables, StylePass::Variables, inheritedFont, inheritedFont);
    applyDeclarations(style, scratch.agent, StylePass::Fonts, inheritedFont, inheritedFont);
    if (fontExplicit_) {
        style.fontSize = font_.size();
        style.fontFamily = font_.family();
    }
    applyDeclarations(style, scratch.author, StylePass::Fonts, inheritedFont, inheritedFont);
    applyDeclarations(style, scratch.agent, StylePass::Rest, inheritedFont, style.fontSize);
    if (backgroundExplicit_) {
        style.background.color = background_;
        style.background.hasColor = true;
        style.background.gradient = false;
        style.background.stopCount = 0;
        style.background.visible = background_.a > 0.f;
    }
    if (fillExplicit_) {
        style.color = textFill_;
    }
    if (subpixelExplicit_) {
        style.subpixel = subpixel_;
    }
    applyDeclarations(style, scratch.author, StylePass::Rest, inheritedFont, style.fontSize);
    style.cursor =
        ResolvedNodeCursor(scratch.agent, scratch.author, inherited.cursor, defaultCursor_, cursorExplicit_, cursor_, isDisabled());

    const ComputedStyle target = style;
    const TransitionTiming backgroundTiming = TimingOf(target, PropertyId::BackgroundColor);
    const TransitionTiming imageTiming = TimingOf(target, PropertyId::BackgroundImage);
    const TransitionTiming colorTiming = TimingOf(target, PropertyId::Color);
    const TransitionTiming borderColorTiming = TimingOf(target, PropertyId::BorderColor);
    const TransitionTiming borderTiming = TimingOf(target, PropertyId::BorderWidth);
    const TransitionTiming shadowTiming = TimingOf(target, PropertyId::BoxShadow);
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

    const bool inheritChanged = !SameInheritable(computed_, style);
    const bool layoutChanged = LayoutAffectingChange(computed_, style);
    // usingAlignment reads ancestors' alignment, so descendants may move as well.
    const bool alignmentChanged =
        computed_.alignment != style.alignment || computed_.alignmentFromCss != style.alignmentFromCss;
    computed_ = style;
    if (layoutChanged) {
        // A node restyled partway through a pass, as a cell rebound with applyCss,
        // measures fresh.
        measure_.clear();
    }
    if (incrementalActive()) {
        ++restyleCount_;
        if (alignmentChanged) {
            markSubtreeLayoutDirty();
        } else if (layoutChanged) {
            markLayoutDirty(LayoutDirt::Size);
        }
    }
    styleDidApply();
    return inheritChanged;
}

void Node::applyStyles(const ComputedStyle& inherited, double timeSeconds, StyleForce force) {
    if (!incrementalActive()) {
        force = StyleForce::Subtree;
    }
    const bool restyle = force != StyleForce::None || styleDirty_;
    if (!restyle && !childStyleDirty_) {
        return;
    }
    StyleForce childForce =
        force == StyleForce::Subtree || styleSubtreeDirty_ ? StyleForce::Subtree : StyleForce::None;
    // Cleared before the work, so a mark made while styling holds for the next frame.
    styleDirty_ = false;
    styleSubtreeDirty_ = false;
    childStyleDirty_ = false;
    if (restyle && resolveStyle(inherited, timeSeconds) && childForce == StyleForce::None) {
        childForce = StyleForce::Self;
    }
    const ComputedStyle pass = asSubScene() != nullptr ? rootInheritance() : inheritableStyle();
    ScratchLease lease;
    std::vector<Node*>& kids = (*lease).kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        child->applyStyles(pass, timeSeconds, childForce);
    }
}


Color Node::themeColor(ThemeColor color) const {
    const auto at = static_cast<std::size_t>(color);
    ThemeCacheEntry& entry = ThemeCacheFor(computed_.variables);
    if (entry.state[at] == 0) {
        const std::string value = computed_.variable(Theme::variableName(color));
        constexpr std::string_view kCurrentColor = "currentcolor";
        if (std::equal(value.begin(), value.end(), kCurrentColor.begin(), kCurrentColor.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            })) {
            entry.state[at] = 2;
        } else {
            bool ok = false;
            const Color parsed = value.empty() ? Color() : Color::parse(value, &ok);
            entry.colors[at] = ok ? parsed : Theme::defaultColor(color);
            entry.state[at] = 1;
        }
    }
    return entry.state[at] == 2 ? computed_.color : entry.colors[at];
}

ComputedStyle Node::inheritableStyle() const {
    ComputedStyle pass;
    pass.color = computed_.color;
    pass.fontSize = computed_.fontSize > 0.f ? computed_.fontSize : 16.f;
    pass.fontFamily = computed_.fontFamily.empty() ? "Open Sans" : computed_.fontFamily;
    pass.subpixel = computed_.subpixel;
    pass.cursor = computed_.cursor;
    pass.variables = computed_.variables;
    return pass;
}

ComputedStyle Node::rootInheritance() {
    ComputedStyle inherited;
    inherited.color = Theme::defaultColor(ThemeColor::Text);
    inherited.fontSize = 16.f;
    inherited.fontFamily = "Open Sans";
    inherited.cursor = Cursor::Default;
    return inherited;
}

ComputedStyle Node::inheritedFromParent() const {
    if (parent_ == nullptr) {
        return ComputedStyle{};
    }
    return parent_->asSubScene() != nullptr ? rootInheritance() : parent_->inheritableStyle();
}

const Stylesheet* Node::userAgentSheet() const {
    for (const Node* node = parent_; node != nullptr; node = node->parent_) {
        if (const SubScene* sub = node->asSubScene()) {
            return &sub->userAgentStylesheet();
        }
    }
    return scene_ != nullptr ? &scene_->userAgentStylesheet() : nullptr;
}

void Node::applyCss() {
    const double time = scene_ != nullptr ? scene_->timeSeconds() : 0.0;
    applyStyles(inheritedFromParent(), time, StyleForce::Subtree);
}

void Node::addFocusWithin(int delta) {
    for (Node* node = this; node != nullptr; node = node->parent_) {
        const bool was = node->focusWithinCount_ > 0;
        node->focusWithinCount_ += delta;
        if (was != (node->focusWithinCount_ > 0)) {
            node->markStyleDirty(focusWithinReachesDescendants() ? StyleDirt::Subtree : StyleDirt::Self);
        }
    }
}

void Node::setFocusedFlag(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    markStyleDirty(StyleDirt::Subtree);
    addFocusWithin(focused ? 1 : -1);
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
    return pickOnBounds_ ? this : nullptr;
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
    return pickOnBounds_ ? this : nullptr;
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
        node->markStyleDirty(StyleDirt::Subtree);
        node->handleHoverChanged();
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
    std::vector<Node*> chain;
    for (Node* node = hit; node != nullptr; node = node->parent_) {
        chain.push_back(node);
    }
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        node->setPressed(std::find(chain.begin(), chain.end(), node) != chain.end());
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
}

void Node::clearFocus(Node* keep) {
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        if (node != keep) {
            node->setFocusedFlag(false);
        }
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
}

void Node::markFocused(Node* hit) {
    if (hit != nullptr) {
        hit->setFocusedFlag(true);
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

    if (backgroundImage_ && backgroundImage_->data_ && backgroundImageOpacity_ > 0.f) {
        renderer.drawImage(backgroundImage_->data_, x + borderLeft, y + borderTop,
                           std::max(0.f, w - borderLeft - borderRight), std::max(0.f, h - borderTop - borderBottom),
                           opacity * backgroundImageOpacity_);
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
