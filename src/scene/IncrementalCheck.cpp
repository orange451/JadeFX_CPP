#include "scene/IncrementalCheck.hpp"

#include <algorithm>
#include <cstdlib>

namespace jadefx {
namespace {

bool SameColor(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool SameSize(const SizeSpec& a, const SizeSpec& b) {
    return a.kind == b.kind && a.pixels == b.pixels && a.percent == b.percent && a.em == b.em;
}

bool SameInsets(const Insets& a, const Insets& b) {
    return a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.left == b.left;
}

bool SameBackground(const Background& a, const Background& b) {
    if (!SameColor(a.color, b.color) || a.hasColor != b.hasColor || a.stopCount != b.stopCount ||
        a.angleDeg != b.angleDeg || a.gradient != b.gradient || a.visible != b.visible) {
        return false;
    }
    const int stops = std::min(a.stopCount, kMaxGradientStops);
    for (int i = 0; i < stops; ++i) {
        if (!SameColor(a.stops[i], b.stops[i]) || a.stopAt[i] != b.stopAt[i]) {
            return false;
        }
    }
    return true;
}

bool SameShadows(const std::vector<BoxShadow>& a, const std::vector<BoxShadow>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].offsetX != b[i].offsetX || a[i].offsetY != b[i].offsetY || a[i].blur != b[i].blur ||
            a[i].spread != b[i].spread || a[i].inset != b[i].inset || !SameColor(a[i].color, b[i].color)) {
            return false;
        }
    }
    return true;
}

bool SameTransitions(const std::shared_ptr<const TransitionTable>& a, const std::shared_ptr<const TransitionTable>& b) {
    if (a == b) {
        return true;
    }
    const TransitionTable empty;
    const TransitionTable& x = a ? *a : empty;
    const TransitionTable& y = b ? *b : empty;
    for (std::size_t i = 0; i < kPropertyIdCount; ++i) {
        if (x.set[i] != y.set[i]) {
            return false;
        }
        if (x.set[i] && (x.timing[i].duration != y.timing[i].duration || x.timing[i].delay != y.timing[i].delay)) {
            return false;
        }
    }
    return true;
}

bool SameVariables(const std::shared_ptr<const CssVariables>& a, const std::shared_ptr<const CssVariables>& b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return (!a || a->empty()) && (!b || b->empty());
    }
    return *a == *b;
}

std::string Segment(const Node& node, int index) {
    std::string text = node.getElementType();
    if (!node.getElementId().empty()) {
        text += "#" + node.getElementId();
    }
    for (const std::string& name : node.getClassList().items()) {
        text += "." + name;
    }
    return text + "[" + std::to_string(index) + "]";
}

}  // namespace

bool IncrementalCheck::requested() {
#ifdef NDEBUG
    return false;
#else
    static const bool on = [] {
        const char* value = std::getenv("JADEFX_VERIFY_INCREMENTAL");
        return value != nullptr && std::string(value) == "1";
    }();
    return on;
#endif
}

bool IncrementalCheck::walkFocusWithin(Node& node) {
    if (node.focused_) {
        return true;
    }
    bool found = false;
    node.visitChildren([&](Node* child) { found = found || walkFocusWithin(*child); });
    return found;
}

void IncrementalCheck::collect(Node& node, const std::string& path, std::vector<NodeSnapshot>& out) {
    NodeSnapshot shot;
    shot.node = &node;
    shot.path = path;
    shot.style = node.computed_;
    shot.x = node.x_;
    shot.y = node.y_;
    shot.width = node.width_;
    shot.height = node.height_;
    shot.animating = node.layoutDirty_;
    shot.focusWithin = node.isFocusWithin();
    shot.focusWithinWalk = walkFocusWithin(node);
    out.push_back(std::move(shot));
    int index = 0;
    node.visitChildren([&](Node* child) { collect(*child, path + " > " + Segment(*child, index++), out); });
}

std::vector<NodeSnapshot> IncrementalCheck::snapshot(Node& root) {
    std::vector<NodeSnapshot> out;
    collect(root, Segment(root, 0), out);
    return out;
}

const char* IncrementalCheck::firstStyleDifference(const ComputedStyle& a, const ComputedStyle& b) {
    if (!SameBackground(a.background, b.background)) return "background";
    if (!SameColor(a.color, b.color)) return "color";
    if (!SameColor(a.borderColor, b.borderColor)) return "border-color";
    if (a.fontSize != b.fontSize) return "font-size";
    if (a.fontFamily != b.fontFamily) return "font-family";
    if (a.subpixel != b.subpixel) return "font-smoothing";
    for (int i = 0; i < 4; ++i) {
        if (!SameSize(a.radius[i], b.radius[i])) return "border-radius";
    }
    if (!SameInsets(a.padding, b.padding)) return "padding";
    if (!SameInsets(a.border, b.border)) return "border-width";
    if (a.borderStyle != b.borderStyle) return "border-style";
    if (!SameShadows(a.shadows, b.shadows)) return "box-shadow";
    if (a.spacing != b.spacing) return "spacing";
    if (a.rowGap != b.rowGap) return "row-gap";
    if (a.columnGap != b.columnGap) return "column-gap";
    if (a.alignment != b.alignment || a.alignmentFromCss != b.alignmentFromCss) return "alignment";
    if (a.opacity != b.opacity) return "opacity";
    if (a.imageColorSet != b.imageColorSet || a.imageColorCurrent != b.imageColorCurrent ||
        !SameColor(a.imageColor, b.imageColor)) return "image-color";
    if (a.cursor != b.cursor) return "cursor";
    if (!SameSize(a.width, b.width)) return "width";
    if (!SameSize(a.height, b.height)) return "height";
    if (!SameSize(a.minWidth, b.minWidth)) return "min-width";
    if (!SameSize(a.minHeight, b.minHeight)) return "min-height";
    if (!SameSize(a.maxWidth, b.maxWidth)) return "max-width";
    if (!SameSize(a.maxHeight, b.maxHeight)) return "max-height";
    if (a.orientationFromCss != b.orientationFromCss || a.orientation != b.orientation) return "orientation";
    if (a.indeterminateBarLengthSet != b.indeterminateBarLengthSet ||
        !SameSize(a.indeterminateBarLength, b.indeterminateBarLength) ||
        a.indeterminateBarEscapeSet != b.indeterminateBarEscapeSet ||
        a.indeterminateBarEscape != b.indeterminateBarEscape || a.indeterminateBarFlipSet != b.indeterminateBarFlipSet ||
        a.indeterminateBarFlip != b.indeterminateBarFlip ||
        a.indeterminateBarAnimationTimeSet != b.indeterminateBarAnimationTimeSet ||
        a.indeterminateBarAnimationTime != b.indeterminateBarAnimationTime) return "indeterminate-bar";
    if (!SameTransitions(a.transitions, b.transitions)) return "transition";
    if (!SameVariables(a.variables, b.variables)) return "custom properties";
    return nullptr;
}

std::string IncrementalCheck::compare(const std::vector<NodeSnapshot>& incremental,
                                      const std::vector<NodeSnapshot>& full) {
    std::string skipBelow;
    const std::size_t count = std::min(incremental.size(), full.size());
    for (std::size_t i = 0; i < count; ++i) {
        const NodeSnapshot& a = incremental[i];
        const NodeSnapshot& b = full[i];
        if (a.node != b.node) {
            return b.path + ": the tree changed during the full pass";
        }
        if (const char* field = firstStyleDifference(a.style, b.style)) {
            return a.path + ": " + field;
        }
        if (a.focusWithin != a.focusWithinWalk) {
            return a.path + ": focus-within flag";
        }
        const bool skipped = !skipBelow.empty() && a.path.rfind(skipBelow, 0) == 0;
        if (!skipped) {
            skipBelow.clear();
            if (a.animating) {
                skipBelow = a.path;
                continue;
            }
            if (a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height) {
                return a.path + ": bounds";
            }
        }
    }
    if (incremental.size() != full.size()) {
        return "scene: the node count changed during the full pass";
    }
    return {};
}

std::string IncrementalCheck::verify(Scene& scene) {
    const std::vector<NodeSnapshot> incremental = snapshot(scene);
    Node::beginLayoutPass();
    Node::setFullPass(true);
    scene.stylePass(scene.lastTime_);
    scene.placePass();
    Node::setFullPass(false);
    Node::endLayoutPass();
    return compare(incremental, snapshot(scene));
}

}  // namespace jadefx
