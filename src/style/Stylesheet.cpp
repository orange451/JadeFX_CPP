#include "jadefx/style/Style.hpp"

#include "jadefx/scene/Node.hpp"
#include "internal/Text.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace jadefx {
namespace {

enum class Combinator { Descendant, Child };

struct Compound {
    std::string type;
    std::string id;
    std::vector<std::string> classes;
    bool hover = false;
    bool active = false;
    bool focus = false;
    bool focusWithin = false;
    bool selected = false;
    bool disabled = false;
    // Other pseudo-classes, such as :vertical or :empty, name a Node::setPseudoState state.
    std::vector<std::string> states;
    // :nth-child(An+B), with odd as 2n+1 and even as 2n.
    struct Nth {
        int a = 0;
        int b = 0;
    };
    std::vector<Nth> nthChild;
    bool universal = false;
};

struct Selector {
    std::vector<Compound> compounds;
    std::vector<Combinator> combinators;
    bool valid = false;
    // Ids count 10000, classes and pseudo-classes 100, and types 1.
    int specificity = 0;
};

int SpecificityOf(const Compound& compound) {
    int classes = static_cast<int>(compound.classes.size() + compound.states.size() + compound.nthChild.size());
    for (const bool pseudo : {compound.hover, compound.active, compound.focus, compound.focusWithin,
                              compound.selected, compound.disabled}) {
        classes += pseudo ? 1 : 0;
    }
    return (compound.id.empty() ? 0 : 10000) + classes * 100 + (compound.type.empty() ? 0 : 1);
}

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
};

struct ParsedLength {
    bool ok = false;
    double px = 0;
    double percent = 0;
    double em = 0;
};

std::vector<std::string> SplitDepth(std::string_view text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (char ch : text) {
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        }
        if (ch == separator && depth == 0) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

bool ParseNumber(std::string_view text, double& value, std::size_t& consumed) {
    const std::string trimmed = trimCopy(text);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    value = std::strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str()) {
        return false;
    }
    consumed = static_cast<std::size_t>(end - trimmed.c_str());
    return true;
}

bool IsIdentChar(unsigned char ch) { return std::isalnum(ch) || ch == '-' || ch == '_'; }

std::string ReadIdent(std::string_view text, std::size_t& index) {
    const std::size_t begin = index;
    while (index < text.size() && IsIdentChar(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
    return std::string(text.substr(begin, index - begin));
}

void SkipSpace(std::string_view text, std::size_t& index) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
}

// A length is a number plus an optional unit. Unknown units fail instead of
// being kept as pixels, which is what turned `1.2em` into 1.2px.
ParsedLength ParseLength(std::string_view text) {
    ParsedLength parsed;
    const std::string trimmed = trimCopy(text);
    if (trimmed.empty()) {
        return parsed;
    }
    char* end = nullptr;
    const double value = std::strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str()) {
        return parsed;
    }
    const std::string unit = lowerCopy(trimCopy(std::string(end)));
    if (unit.empty() || unit == "px") {
        parsed.ok = true;
        parsed.px = value;
    } else if (unit == "em") {
        parsed.ok = true;
        parsed.em = value;
    } else if (unit == "%") {
        parsed.ok = true;
        parsed.percent = value / 100.0;
    }
    return parsed;
}

double ResolveLength(const ParsedLength& length, float emFontSize) {
    return length.px + length.em * static_cast<double>(emFontSize);
}

SizeSpec LengthToSpec(std::string_view text) {
    const std::string trimmed = trimCopy(text);
    const std::string lower = lowerCopy(trimmed);
    if (lower.rfind("calc(", 0) == 0 && lower.size() > 6 && lower.back() == ')') {
        const std::string inner = trimmed.substr(5, trimmed.size() - 6);
        std::size_t op = std::string::npos;
        int depth = 0;
        for (std::size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '(') {
                ++depth;
            } else if (inner[i] == ')') {
                --depth;
            } else if (depth == 0 && (inner[i] == '+' || inner[i] == '-') && i > 0) {
                op = i;
            }
        }
        if (op == std::string::npos) {
            return SizeSpec::unset();
        }
        const double sign = inner[op] == '-' ? -1.0 : 1.0;
        const ParsedLength left = ParseLength(inner.substr(0, op));
        const ParsedLength right = ParseLength(inner.substr(op + 1));
        if (!left.ok || !right.ok) {
            return SizeSpec::unset();
        }
        SizeSpec spec;
        spec.percent = left.percent + sign * right.percent;
        spec.pixels = left.px + sign * right.px;
        spec.em = left.em + sign * right.em;
        spec.kind = (spec.percent != 0.0) ? SizeKind::Calc : SizeKind::Pixels;
        if (spec.kind == SizeKind::Pixels && spec.pixels == 0.0 && spec.em == 0.0 && spec.percent == 0.0) {
            spec.kind = SizeKind::Pixels;
        }
        return spec;
    }

    const ParsedLength length = ParseLength(trimmed);
    if (!length.ok) {
        return SizeSpec::unset();
    }
    SizeSpec spec;
    spec.pixels = length.px;
    spec.percent = length.percent;
    spec.em = length.em;
    if (length.percent != 0.0 && (length.px != 0.0 || length.em != 0.0)) {
        spec.kind = SizeKind::Calc;
    } else if (length.percent != 0.0) {
        spec.kind = SizeKind::Percent;
    } else {
        spec.kind = SizeKind::Pixels;
    }
    return spec;
}

void SetSize(SizeSpec& slot, std::string_view text) {
    const SizeSpec spec = LengthToSpec(text);
    if (spec.set()) {
        slot = spec;
    }
}

bool HasClass(const Node& node, const std::string& name) {
    for (const std::string& have : node.getClassList().items()) {
        if (have == name) {
            return true;
        }
    }
    return false;
}

bool MatchCompound(const Compound& compound, Node& node) {
    if (!compound.type.empty() && compound.type != node.getElementType()) {
        return false;
    }
    if (!compound.id.empty() && compound.id != node.getElementId()) {
        return false;
    }
    for (const std::string& name : compound.classes) {
        if (!HasClass(node, name)) {
            return false;
        }
    }
    if (compound.hover && !node.isHovered()) {
        return false;
    }
    if (compound.active && !node.isPressed()) {
        return false;
    }
    if (compound.focus && !node.isFocused()) {
        return false;
    }
    if (compound.focusWithin && !node.isFocusWithin()) {
        return false;
    }
    if (compound.selected && !node.isSelected()) {
        return false;
    }
    if (compound.disabled && !node.isDisabled()) {
        return false;
    }
    for (const std::string& state : compound.states) {
        if (!node.pseudoState(state)) {
            return false;
        }
    }
    for (const Compound::Nth& nth : compound.nthChild) {
        // Some n >= 0 gives a*n + b == position.
        const int position = node.getNthChildIndex();
        if (position < 1) {
            return false;
        }
        const int rest = position - nth.b;
        if (nth.a == 0 ? rest != 0 : (rest % nth.a != 0 || rest / nth.a < 0)) {
            return false;
        }
    }
    return true;
}

bool MatchAt(const Selector& selector, std::size_t index, Node* node) {
    if (node == nullptr || !MatchCompound(selector.compounds[index], *node)) {
        return false;
    }
    if (index == 0) {
        return true;
    }
    const Combinator combinator = selector.combinators[index - 1];
    if (combinator == Combinator::Child) {
        return MatchAt(selector, index - 1, node->getParent());
    }
    for (Node* parent = node->getParent(); parent != nullptr; parent = parent->getParent()) {
        if (MatchAt(selector, index - 1, parent)) {
            return true;
        }
    }
    return false;
}

bool Matches(const Selector& selector, Node& node) {
    if (!selector.valid || selector.compounds.empty()) {
        return false;
    }
    return MatchAt(selector, selector.compounds.size() - 1, &node);
}

// An+B from :nth-child(), as CSS Selectors level 3 writes it: odd, even, 3,
// 2n, 2n+1, -n+3, n. Spaces around the sign are allowed.
bool ParseNth(std::string_view text, int& a, int& b) {
    std::string compact;
    for (const char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            compact.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (compact == "odd") {
        a = 2;
        b = 1;
        return true;
    }
    if (compact == "even") {
        a = 2;
        b = 0;
        return true;
    }
    auto readInt = [](const std::string& digits, int& out) {
        if (digits.empty() || digits.size() > 6) {
            return false;
        }
        for (const char ch : digits) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return false;
            }
        }
        out = std::stoi(digits);
        return true;
    };
    const std::size_t n = compact.find('n');
    if (n == std::string::npos) {
        a = 0;
        const bool negative = !compact.empty() && compact[0] == '-';
        const std::size_t start = !compact.empty() && (compact[0] == '-' || compact[0] == '+') ? 1 : 0;
        if (!readInt(compact.substr(start), b)) {
            return false;
        }
        b = negative ? -b : b;
        return true;
    }
    const std::string head = compact.substr(0, n);
    if (head.empty() || head == "+") {
        a = 1;
    } else if (head == "-") {
        a = -1;
    } else {
        const bool negative = head[0] == '-';
        const std::size_t start = head[0] == '-' || head[0] == '+' ? 1 : 0;
        if (!readInt(head.substr(start), a)) {
            return false;
        }
        a = negative ? -a : a;
    }
    const std::string tail = compact.substr(n + 1);
    b = 0;
    if (tail.empty()) {
        return true;
    }
    if (tail[0] != '+' && tail[0] != '-') {
        return false;
    }
    if (!readInt(tail.substr(1), b)) {
        return false;
    }
    b = tail[0] == '-' ? -b : b;
    return true;
}

bool ParseCompound(std::string_view text, std::size_t& index, Compound& compound) {
    bool any = false;
    while (index < text.size()) {
        const char ch = text[index];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '>' || ch == '+' || ch == '~' || ch == ',') {
            break;
        }
        if (ch == '*') {
            compound.universal = true;
            any = true;
            ++index;
            continue;
        }
        if (ch == '#') {
            ++index;
            compound.id = ReadIdent(text, index);
            if (compound.id.empty()) {
                return false;
            }
            any = true;
            continue;
        }
        if (ch == '.') {
            ++index;
            const std::string name = ReadIdent(text, index);
            if (name.empty()) {
                return false;
            }
            compound.classes.push_back(name);
            any = true;
            continue;
        }
        if (ch == ':') {
            ++index;
            if (index < text.size() && text[index] == ':') {
                return false;
            }
            const std::string pseudo = lowerCopy(ReadIdent(text, index));
            if (pseudo.empty()) {
                return false;
            }
            if (index < text.size() && text[index] == '(') {
                const std::size_t close = text.find(')', index);
                Compound::Nth nth;
                if (pseudo != "nth-child" || close == std::string_view::npos ||
                    !ParseNth(text.substr(index + 1, close - index - 1), nth.a, nth.b)) {
                    return false;
                }
                compound.nthChild.push_back(nth);
                index = close + 1;
                any = true;
                continue;
            }
            if (pseudo == "hover") {
                compound.hover = true;
            } else if (pseudo == "active") {
                compound.active = true;
            } else if (pseudo == "focus") {
                compound.focus = true;
            } else if (pseudo == "focus-within") {
                compound.focusWithin = true;
            } else if (pseudo == "select" || pseudo == "selected") {
                compound.selected = true;
            } else if (pseudo == "disabled") {
                compound.disabled = true;
            } else {
                compound.states.push_back(pseudo);
            }
            any = true;
            continue;
        }
        if (IsIdentChar(static_cast<unsigned char>(ch))) {
            compound.type = lowerCopy(ReadIdent(text, index));
            if (compound.type.empty()) {
                return false;
            }
            any = true;
            continue;
        }
        return false;
    }
    return any;
}

Selector ParseSelector(std::string_view text) {
    Selector selector;
    std::size_t index = 0;
    SkipSpace(text, index);
    Compound compound;
    if (!ParseCompound(text, index, compound)) {
        return selector;
    }
    selector.compounds.push_back(std::move(compound));
    while (index < text.size()) {
        const std::size_t beforeSpace = index;
        SkipSpace(text, index);
        const bool sawSpace = index > beforeSpace;
        Combinator combinator = Combinator::Descendant;
        if (index < text.size() && (text[index] == '+' || text[index] == '~')) {
            return selector;
        }
        if (index < text.size() && text[index] == '>') {
            combinator = Combinator::Child;
            ++index;
            SkipSpace(text, index);
        } else if (!sawSpace) {
            break;
        }
        Compound next;
        if (!ParseCompound(text, index, next)) {
            return selector;
        }
        selector.combinators.push_back(combinator);
        selector.compounds.push_back(std::move(next));
    }
    SkipSpace(text, index);
    selector.valid = index == text.size();
    for (const Compound& part : selector.compounds) {
        selector.specificity += SpecificityOf(part);
    }
    return selector;
}

void PrepareDeclaration(Declaration& declaration);

std::vector<Declaration> ParseDeclarations(std::string_view body) {
    std::vector<Declaration> declarations;
    for (const std::string& part : SplitDepth(body, ';')) {
        const std::string item = trimCopy(part);
        if (item.empty()) {
            continue;
        }
        const std::size_t colon = item.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        Declaration declaration;
        // Custom property names are case-sensitive, as in CSS.
        declaration.property = trimCopy(item.substr(0, colon));
        if (declaration.property.rfind("--", 0) != 0) {
            declaration.property = lowerCopy(declaration.property);
        }
        declaration.value = trimCopy(item.substr(colon + 1));
        const std::size_t bang = declaration.value.rfind('!');
        if (bang != std::string::npos && lowerCopy(trimCopy(declaration.value.substr(bang + 1))) == "important") {
            declaration.important = true;
            declaration.value = trimCopy(declaration.value.substr(0, bang));
        }
        if (!declaration.property.empty() && !declaration.value.empty()) {
            PrepareDeclaration(declaration);
            declarations.push_back(std::move(declaration));
        }
    }
    return declarations;
}

bool ParseColorPrefix(std::string_view text, Color& color, std::size_t& consumed) {
    std::size_t index = 0;
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
    if (index >= text.size()) {
        return false;
    }
    if (text[index] == '#') {
        std::size_t end = index + 1;
        while (end < text.size() && std::isxdigit(static_cast<unsigned char>(text[end]))) {
            ++end;
        }
        bool ok = false;
        color = Color::parse(text.substr(index, end - index), &ok);
        if (!ok) {
            return false;
        }
        consumed = end;
        return true;
    }
    const std::string lower = lowerCopy(std::string(text.substr(index)));
    if (lower.rfind("rgb", 0) == 0) {
        const std::size_t open = text.find('(', index);
        if (open == std::string_view::npos) {
            return false;
        }
        int depth = 0;
        std::size_t end = open;
        for (; end < text.size(); ++end) {
            if (text[end] == '(') {
                ++depth;
            } else if (text[end] == ')') {
                --depth;
                if (depth == 0) {
                    ++end;
                    break;
                }
            }
        }
        bool ok = false;
        color = Color::parse(text.substr(index, end - index), &ok);
        if (!ok) {
            return false;
        }
        consumed = end;
        return true;
    }
    const std::size_t begin = index;
    const std::string name = ReadIdent(text, index);
    if (name.empty()) {
        return false;
    }
    bool ok = false;
    color = Color::parse(name, &ok);
    if (!ok) {
        index = begin;
        return false;
    }
    consumed = index;
    return true;
}

float DirectionAngle(std::string_view text) {
    std::string value = lowerCopy(trimCopy(text));
    if (value.rfind("to ", 0) == 0) {
        value = value.substr(3);
    }
    const bool top = value.find("top") != std::string::npos;
    const bool bottom = value.find("bottom") != std::string::npos;
    const bool left = value.find("left") != std::string::npos;
    const bool right = value.find("right") != std::string::npos;
    if (top && right) {
        return 45.f;
    }
    if (bottom && right) {
        return 135.f;
    }
    if (bottom && left) {
        return 225.f;
    }
    if (top && left) {
        return 315.f;
    }
    if (right) {
        return 90.f;
    }
    if (left) {
        return 270.f;
    }
    if (top) {
        return 0.f;
    }
    return 180.f;
}

struct GradientStop {
    Color color;
    bool hasPosition = false;
    float position = 0.f;
};

void PlaceStops(std::vector<GradientStop>& stops) {
    if (stops.empty()) {
        return;
    }
    const bool any = std::any_of(stops.begin(), stops.end(), [](const GradientStop& stop) { return stop.hasPosition; });
    if (!any) {
        if (stops.size() == 1) {
            stops[0].position = 0.f;
            return;
        }
        for (std::size_t i = 0; i < stops.size(); ++i) {
            stops[i].position = static_cast<float>(i) / static_cast<float>(stops.size() - 1);
        }
        return;
    }
    if (!stops.front().hasPosition) {
        stops.front().position = 0.f;
        stops.front().hasPosition = true;
    }
    if (!stops.back().hasPosition) {
        stops.back().position = 1.f;
        stops.back().hasPosition = true;
    }
    for (std::size_t i = 0; i < stops.size();) {
        if (stops[i].hasPosition) {
            ++i;
            continue;
        }
        const std::size_t start = i;
        while (i < stops.size() && !stops[i].hasPosition) {
            ++i;
        }
        const float left = stops[start - 1].position;
        const float right = stops[i].position;
        const float step = (right - left) / static_cast<float>(i - start + 1);
        for (std::size_t j = start; j < i; ++j) {
            stops[j].position = left + step * static_cast<float>(j - start + 1);
            stops[j].hasPosition = true;
        }
    }
}

void ApplyBackgroundImage(ComputedStyle& style, std::string_view text) {
    const std::string lower = lowerCopy(std::string(text));
    const std::size_t start = lower.find("linear-gradient");
    if (start == std::string::npos) {
        return;
    }
    const std::size_t open = text.find('(', start);
    const std::size_t close = text.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        return;
    }
    const std::vector<std::string> args = SplitDepth(text.substr(open + 1, close - open - 1), ',');
    if (args.empty()) {
        return;
    }
    std::size_t colorStart = 0;
    const std::string first = lowerCopy(trimCopy(args[0]));
    if (first.rfind("to ", 0) == 0) {
        style.background.angleDeg = DirectionAngle(first);
        colorStart = 1;
    } else if (first.find("deg") != std::string::npos) {
        double angle = 180;
        std::size_t consumed = 0;
        ParseNumber(first, angle, consumed);
        style.background.angleDeg = static_cast<float>(angle);
        colorStart = 1;
    } else {
        style.background.angleDeg = 180.f;
    }
    std::vector<GradientStop> parsed;
    for (std::size_t i = colorStart; i < args.size() && parsed.size() < static_cast<std::size_t>(kMaxGradientStops); ++i) {
        const std::string arg = trimCopy(args[i]);
        Color color;
        std::size_t consumed = 0;
        if (!ParseColorPrefix(arg, color, consumed)) {
            return;
        }
        GradientStop stop;
        stop.color = color;
        const std::string rest = trimCopy(arg.substr(consumed));
        if (!rest.empty()) {
            const ParsedLength position = ParseLength(rest);
            const bool zero = rest == "0" || rest == "0%" || rest == "0px";
            if (!position.ok || position.em != 0.0 || (position.px != 0.0 && rest.find('%') == std::string::npos && !zero)) {
                return;
            }
            stop.hasPosition = true;
            stop.position = zero ? 0.f : static_cast<float>(position.percent);
        }
        parsed.push_back(stop);
    }
    if (parsed.size() < 2) {
        return;
    }
    PlaceStops(parsed);
    style.background.stopCount = static_cast<int>(parsed.size());
    for (std::size_t i = 0; i < parsed.size(); ++i) {
        style.background.stops[i] = parsed[i].color;
        style.background.stopAt[i] = parsed[i].position;
    }
    style.background.gradient = true;
    style.background.visible = true;
}

Pos ParseAlignment(std::string_view text) {
    std::string value = lowerCopy(trimCopy(text));
    for (char& ch : value) {
        if (ch == '_' || ch == '-') {
            ch = ' ';
        }
    }
    if (value == "center") {
        return Pos::Center;
    }
    if (value == "top" || value == "top center" || value == "center top") {
        return Pos::TopCenter;
    }
    if (value == "bottom" || value == "bottom center" || value == "center bottom") {
        return Pos::BottomCenter;
    }
    if (value == "left" || value == "center left" || value == "left center") {
        return Pos::CenterLeft;
    }
    if (value == "right" || value == "center right" || value == "right center") {
        return Pos::CenterRight;
    }
    if (value == "top left" || value == "left top") {
        return Pos::TopLeft;
    }
    if (value == "top right" || value == "right top") {
        return Pos::TopRight;
    }
    if (value == "bottom left" || value == "left bottom") {
        return Pos::BottomLeft;
    }
    if (value == "bottom right" || value == "right bottom") {
        return Pos::BottomRight;
    }
    return Pos::Center;
}

std::vector<std::string> ShadowTokens(std::string_view text);

bool IsEasing(std::string_view token) {
    const std::string lower = lowerCopy(trimCopy(token));
    return lower == "ease" || lower == "ease-in" || lower == "ease-out" || lower == "ease-in-out" || lower == "linear" ||
           lower == "step-start" || lower == "step-end" || lower.rfind("steps(", 0) == 0 ||
           lower.rfind("cubic-bezier(", 0) == 0;
}

bool IsTime(std::string_view token, double& seconds) {
    const std::string lower = lowerCopy(trimCopy(token));
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "ms") {
        const ParsedLength length = ParseLength(lower.substr(0, lower.size() - 2));
        if (!length.ok || length.em != 0.0 || length.percent != 0.0) {
            return false;
        }
        seconds = length.px / 1000.0;
        return true;
    }
    if (!lower.empty() && lower.back() == 's') {
        const ParsedLength length = ParseLength(lower.substr(0, lower.size() - 1));
        if (!length.ok || length.em != 0.0 || length.percent != 0.0) {
            return false;
        }
        seconds = length.px;
        return true;
    }
    return false;
}

void ApplyTransition(ComputedStyle& style, std::string_view text) {
    for (const std::string& part : SplitDepth(text, ',')) {
        std::string property = "all";
        double duration = 0;
        double delay = 0;
        int times = 0;
        bool sawProperty = false;
        for (const std::string& token : ShadowTokens(part)) {
            double seconds = 0;
            if (IsTime(token, seconds)) {
                if (times == 0) {
                    duration = seconds;
                } else if (times == 1) {
                    delay = seconds;
                }
                ++times;
            } else if (IsEasing(token)) {
                continue;
            } else if (!sawProperty) {
                property = lowerCopy(token);
                sawProperty = true;
            }
        }
        if (duration < 0) {
            duration = 0;
        }
        if (delay < 0) {
            delay = 0;
        }
        style.transitions[property] = {duration, delay};
    }
}

std::vector<std::string> ShadowTokens(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    int depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '(') {
            ++depth;
            current.push_back(ch);
            continue;
        }
        if (ch == ')') {
            if (depth > 0) {
                --depth;
            }
            current.push_back(ch);
            continue;
        }
        if (depth == 0 && std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

BoxShadow ParseShadow(std::string_view text, float emFontSize) {
    BoxShadow shadow;
    std::vector<double> lengths;
    bool sawColor = false;
    for (const std::string& token : ShadowTokens(text)) {
        const std::string lower = lowerCopy(token);
        if (lower == "inset") {
            shadow.inset = true;
            continue;
        }
        if (lower == "none") {
            continue;
        }
        const ParsedLength length = ParseLength(token);
        if (length.ok && length.percent == 0.0) {
            lengths.push_back(ResolveLength(length, emFontSize));
            continue;
        }
        bool ok = false;
        const Color color = Color::parse(token, &ok);
        if (ok) {
            shadow.color = color;
            sawColor = true;
        }
    }
    if (!lengths.empty()) {
        shadow.offsetX = lengths[0];
    }
    if (lengths.size() > 1) {
        shadow.offsetY = lengths[1];
    }
    if (lengths.size() > 2) {
        shadow.blur = lengths[2];
    }
    if (lengths.size() > 3) {
        shadow.spread = lengths[3];
    }
    if (!sawColor) {
        shadow.color = Color::black();
    }
    if (shadow.blur < 0) {
        shadow.blur = 0;
    }
    return shadow;
}

std::vector<ParsedLength> LengthList(std::string_view text) {
    std::vector<ParsedLength> values;
    std::string current;
    bool failed = false;
    auto push = [&]() {
        if (current.empty()) {
            return;
        }
        const ParsedLength length = ParseLength(current);
        if (!length.ok) {
            failed = true;
        } else {
            values.push_back(length);
        }
        current.clear();
    };
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            push();
        } else {
            current.push_back(ch);
        }
    }
    push();
    if (failed) {
        return {};
    }
    return values;
}

Insets BoxFromLengths(const ParsedLength* values, std::size_t count, float emFontSize) {
    double resolved[4] = {};
    const std::size_t used = std::min<std::size_t>(count, 4);
    for (std::size_t i = 0; i < used; ++i) {
        resolved[i] = ResolveLength(values[i], emFontSize);
    }
    if (count == 1) {
        return Insets::uniform(resolved[0]);
    }
    if (count == 2) {
        return Insets::axes(resolved[0], resolved[1]);
    }
    if (count == 3) {
        return {resolved[0], resolved[1], resolved[2], resolved[1]};
    }
    return {resolved[0], resolved[1], resolved[2], resolved[3]};
}

void AssignRadius(ComputedStyle& style, const ParsedLength* values, std::size_t count) {
    if (count == 0) {
        return;
    }
    ParsedLength expanded[4];
    if (count == 1) {
        expanded[0] = expanded[1] = expanded[2] = expanded[3] = values[0];
    } else if (count == 2) {
        expanded[0] = expanded[2] = values[0];
        expanded[1] = expanded[3] = values[1];
    } else if (count == 3) {
        expanded[0] = values[0];
        expanded[1] = expanded[3] = values[1];
        expanded[2] = values[2];
    } else {
        expanded[0] = values[0];
        expanded[1] = values[1];
        expanded[2] = values[2];
        expanded[3] = values[3];
    }
    for (int i = 0; i < 4; ++i) {
        SizeSpec spec;
        spec.pixels = expanded[i].px;
        spec.percent = expanded[i].percent;
        spec.em = expanded[i].em;
        spec.kind = expanded[i].percent != 0.0 ? SizeKind::Percent : SizeKind::Pixels;
        if (expanded[i].percent != 0.0 && (expanded[i].px != 0.0 || expanded[i].em != 0.0)) {
            spec.kind = SizeKind::Calc;
        }
        style.radius[i] = spec;
    }
}

struct PropertyName {
    std::string_view name;
    PropertyId id;
};

constexpr PropertyName kPropertyNames[] = {
    {"font-size", PropertyId::FontSize},
    {"font-family", PropertyId::FontFamily},
    {"background-color", PropertyId::BackgroundColor},
    {"background", PropertyId::Background},
    {"background-image", PropertyId::BackgroundImage},
    {"font-smoothing", PropertyId::FontSmoothing},
    {"color", PropertyId::Color},
    {"image-color", PropertyId::ImageColor},
    {"width", PropertyId::Width},
    {"height", PropertyId::Height},
    {"min-width", PropertyId::MinWidth},
    {"min-height", PropertyId::MinHeight},
    {"max-width", PropertyId::MaxWidth},
    {"max-height", PropertyId::MaxHeight},
    {"border-radius", PropertyId::BorderRadius},
    {"border-width", PropertyId::BorderWidth},
    {"border-color", PropertyId::BorderColor},
    {"border-style", PropertyId::BorderStyle},
    {"box-shadow", PropertyId::BoxShadow},
    {"padding", PropertyId::Padding},
    {"spacing", PropertyId::Spacing},
    {"gap", PropertyId::Gap},
    {"row-gap", PropertyId::RowGap},
    {"column-gap", PropertyId::ColumnGap},
    {"alignment", PropertyId::Alignment},
    {"orientation", PropertyId::Orientation},
    {"opacity", PropertyId::Opacity},
    {"indeterminate-bar-length", PropertyId::IndeterminateBarLength},
    {"indeterminate-bar-escape", PropertyId::IndeterminateBarEscape},
    {"indeterminate-bar-flip", PropertyId::IndeterminateBarFlip},
    {"indeterminate-bar-animation-time", PropertyId::IndeterminateBarAnimationTime},
    {"transition", PropertyId::Transition},
    {"cursor", PropertyId::Cursor},
    {"all", PropertyId::All},
};

void PrepareDeclaration(Declaration& declaration) {
    declaration.id = propertyIdOf(declaration.property);
    declaration.hasVar = declaration.value.find("var(") != std::string::npos;
    if (declaration.hasVar) {
        return;
    }
    DeclarationValue& parsed = declaration.parsed;
    switch (declaration.id) {
        case PropertyId::Color:
        case PropertyId::BackgroundColor:
        case PropertyId::BorderColor: {
            bool ok = false;
            parsed.color = Color::parse(declaration.value, &ok);
            parsed.kind = ok ? DeclarationValue::Kind::Color : DeclarationValue::Kind::Invalid;
            break;
        }
        case PropertyId::Background: {
            // A gradient is parsed as it is applied. A plain color is parsed now.
            if (lowerCopy(declaration.value).find("gradient") != std::string::npos) {
                break;
            }
            bool ok = false;
            parsed.color = Color::parse(declaration.value, &ok);
            parsed.kind = ok ? DeclarationValue::Kind::Color : DeclarationValue::Kind::Invalid;
            break;
        }
        case PropertyId::Width:
        case PropertyId::Height:
        case PropertyId::MinWidth:
        case PropertyId::MinHeight:
        case PropertyId::MaxWidth:
        case PropertyId::MaxHeight:
            parsed.size = LengthToSpec(declaration.value);
            parsed.kind = parsed.size.set() ? DeclarationValue::Kind::Size : DeclarationValue::Kind::Invalid;
            break;
        case PropertyId::Padding:
        case PropertyId::BorderWidth:
        case PropertyId::BorderRadius: {
            const std::vector<ParsedLength> list = LengthList(declaration.value);
            if (list.empty()) {
                parsed.kind = DeclarationValue::Kind::Invalid;
                break;
            }
            // Past four, only the first four count, as BoxFromLengths and AssignRadius read them.
            parsed.kind = DeclarationValue::Kind::Lengths;
            parsed.lengthCount = static_cast<int>(std::min<std::size_t>(list.size(), 4));
            for (int i = 0; i < parsed.lengthCount; ++i) {
                parsed.lengths[i].pixels = list[static_cast<std::size_t>(i)].px;
                parsed.lengths[i].percent = list[static_cast<std::size_t>(i)].percent;
                parsed.lengths[i].em = list[static_cast<std::size_t>(i)].em;
            }
            break;
        }
        default:
            break;
    }
}

bool ColorOf(const Declaration& declaration, const std::string& value, Color& color) {
    switch (declaration.parsed.kind) {
        case DeclarationValue::Kind::Color:
            color = declaration.parsed.color;
            return true;
        case DeclarationValue::Kind::Invalid:
            return false;
        default: {
            bool ok = false;
            color = Color::parse(value, &ok);
            return ok;
        }
    }
}

bool SizeOf(const Declaration& declaration, const std::string& value, SizeSpec& size) {
    switch (declaration.parsed.kind) {
        case DeclarationValue::Kind::Size:
            size = declaration.parsed.size;
            return true;
        case DeclarationValue::Kind::Invalid:
            return false;
        default:
            size = LengthToSpec(value);
            return size.set();
    }
}

// Up to four lengths into out. Returns how many, or 0 when the value does not parse.
std::size_t LengthsOf(const Declaration& declaration, const std::string& value, ParsedLength (&out)[4]) {
    if (declaration.parsed.kind == DeclarationValue::Kind::Lengths) {
        const std::size_t count = static_cast<std::size_t>(declaration.parsed.lengthCount);
        for (std::size_t i = 0; i < count; ++i) {
            const SizeSpec& length = declaration.parsed.lengths[i];
            out[i] = ParsedLength{true, length.pixels, length.percent, length.em};
        }
        return count;
    }
    if (declaration.parsed.kind == DeclarationValue::Kind::Invalid) {
        return 0;
    }
    const std::vector<ParsedLength> list = LengthList(value);
    const std::size_t count = std::min<std::size_t>(list.size(), 4);
    std::copy_n(list.begin(), count, out);
    return count;
}

// A selector, by its rule and its place in that rule.
struct IndexEntry {
    std::uint32_t rule = 0;
    std::uint32_t selector = 0;
};

bool operator<(const IndexEntry& a, const IndexEntry& b) {
    return a.rule != b.rule ? a.rule < b.rule : a.selector < b.selector;
}

bool operator==(const IndexEntry& a, const IndexEntry& b) { return a.rule == b.rule && a.selector == b.selector; }

using IndexBuckets = std::unordered_map<std::string_view, std::vector<IndexEntry>>;

}  // namespace

// Rules, and each selector filed under its rightmost compound: by id when it has
// one, else by its first class, else by its type, else as universal. A node can
// only match selectors filed under its own id, classes, or type, or the universal
// ones. The keys view strings inside rules, which never change after parse.
struct Stylesheet::Data {
    std::vector<Rule> rules;
    IndexBuckets byId;
    IndexBuckets byClass;
    IndexBuckets byType;
    std::vector<IndexEntry> universal;
};

Stylesheet::Stylesheet() : data_(std::make_shared<Data>()) {}

Stylesheet::Stylesheet(const Stylesheet& other) = default;
Stylesheet& Stylesheet::operator=(const Stylesheet& other) = default;
Stylesheet::Stylesheet(Stylesheet&& other) noexcept = default;
Stylesheet& Stylesheet::operator=(Stylesheet&& other) noexcept = default;
Stylesheet::~Stylesheet() = default;

Stylesheet Stylesheet::parse(const std::string& css) {
    Stylesheet sheet;
    const std::string text = stripCssComments(css);
    std::size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
            ++index;
        }
        if (index >= text.size()) {
            break;
        }
        const std::size_t open = text.find('{', index);
        if (open == std::string::npos) {
            break;
        }
        const std::size_t close = text.find('}', open + 1);
        if (close == std::string::npos) {
            break;
        }
        Rule rule;
        rule.declarations = ParseDeclarations(text.substr(open + 1, close - open - 1));
        for (const std::string& part : SplitDepth(text.substr(index, open - index), ',')) {
            const std::string piece = trimCopy(part);
            if (piece.empty()) {
                continue;
            }
            Selector selector = ParseSelector(piece);
            if (selector.valid) {
                rule.selectors.push_back(std::move(selector));
            }
        }
        if (!rule.selectors.empty() && !rule.declarations.empty()) {
            sheet.data_->rules.push_back(std::move(rule));
        }
        index = close + 1;
    }
    Data& data = *sheet.data_;
    for (std::uint32_t r = 0; r < data.rules.size(); ++r) {
        const Rule& rule = data.rules[r];
        for (std::uint32_t s = 0; s < rule.selectors.size(); ++s) {
            const Compound& key = rule.selectors[s].compounds.back();
            const IndexEntry entry{r, s};
            if (!key.id.empty()) {
                data.byId[key.id].push_back(entry);
            } else if (!key.classes.empty()) {
                data.byClass[key.classes.front()].push_back(entry);
            } else if (!key.type.empty()) {
                data.byType[key.type].push_back(entry);
            } else {
                data.universal.push_back(entry);
            }
        }
    }
    return sheet;
}

void Stylesheet::collectMatching(Node& node, std::vector<MatchedDeclaration>& out) const {
    if (!data_ || data_->rules.empty()) {
        return;
    }
    const Data& data = *data_;
    // Reused between calls. Matching never calls back into a stylesheet.
    thread_local std::vector<IndexEntry> candidates;
    candidates.clear();
    auto gather = [&](const IndexBuckets& buckets, std::string_view key) {
        const auto found = buckets.find(key);
        if (found != buckets.end()) {
            candidates.insert(candidates.end(), found->second.begin(), found->second.end());
        }
    };
    if (!node.getElementId().empty()) {
        gather(data.byId, node.getElementId());
    }
    for (const std::string& name : node.getClassList().items()) {
        gather(data.byClass, name);
    }
    gather(data.byType, node.getElementType());
    candidates.insert(candidates.end(), data.universal.begin(), data.universal.end());
    // Source order, as the unindexed scan produced. A class listed twice adds its bucket twice.
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    for (std::size_t i = 0; i < candidates.size();) {
        const std::uint32_t ruleIndex = candidates[i].rule;
        const Rule& rule = data.rules[ruleIndex];
        // A rule with several selectors counts the most specific one that matches.
        int specificity = -1;
        for (; i < candidates.size() && candidates[i].rule == ruleIndex; ++i) {
            const Selector& selector = rule.selectors[candidates[i].selector];
            if (selector.specificity > specificity && Matches(selector, node)) {
                specificity = selector.specificity;
            }
        }
        if (specificity < 0) {
            continue;
        }
        for (const Declaration& declaration : rule.declarations) {
            out.push_back({&declaration, specificity});
        }
    }
}

PropertyId propertyIdOf(std::string_view property) {
    if (property.rfind("--", 0) == 0 || property == "accent-color" || property == "caret-color" ||
        property == "outline-color") {
        return PropertyId::Custom;
    }
    for (const PropertyName& entry : kPropertyNames) {
        if (entry.name == property) {
            return entry.id;
        }
    }
    return PropertyId::Unknown;
}

std::string resolveCssVariables(std::string_view value, const CssVariables* variables) {
    // Bounded, so a variable that names itself cannot loop.
    std::string text(value);
    for (int depth = 0; depth < 16; ++depth) {
        const std::size_t start = text.find("var(");
        if (start == std::string::npos) {
            return text;
        }
        // The matching parenthesis, past any nested var() in a fallback.
        int level = 0;
        std::size_t end = start + 3;
        for (; end < text.size(); ++end) {
            if (text[end] == '(') {
                ++level;
            } else if (text[end] == ')' && --level == 0) {
                break;
            }
        }
        if (end >= text.size()) {
            return {};
        }
        const std::string inside = text.substr(start + 4, end - start - 4);
        const std::size_t comma = inside.find(',');
        const std::string name = trimCopy(inside.substr(0, comma));
        std::string replacement;
        const auto found = variables != nullptr ? variables->find(name) : CssVariables::const_iterator{};
        if (variables != nullptr && found != variables->end()) {
            replacement = found->second;
        } else if (comma != std::string::npos) {
            replacement = trimCopy(inside.substr(comma + 1));
        } else {
            return {};
        }
        text.replace(start, end - start + 1, replacement);
    }
    return text.find("var(") == std::string::npos ? text : std::string();
}

std::string ComputedStyle::variable(std::string_view name) const {
    if (!variables) {
        return {};
    }
    const auto found = variables->find(std::string(name));
    return found == variables->end() ? std::string() : resolveCssVariables(found->second, variables.get());
}

bool Stylesheet::empty() const { return !data_ || data_->rules.empty(); }

namespace {

struct CursorKeyword {
    const char* name;
    Cursor cursor;
};

const CursorKeyword kCursors[] = {
    {"inherit", Cursor::Inherit},
    {"auto", Cursor::Auto},
    {"default", Cursor::Default},
    {"pointer", Cursor::Pointer},
    {"hand", Cursor::Pointer},
    {"text", Cursor::Text},
    {"vertical-text", Cursor::Text},
    {"crosshair", Cursor::Crosshair},
    {"cell", Cursor::Crosshair},
    {"move", Cursor::Move},
    {"all-scroll", Cursor::Move},
    {"not-allowed", Cursor::NotAllowed},
    {"no-drop", Cursor::NotAllowed},
    {"ew-resize", Cursor::EwResize},
    {"e-resize", Cursor::EwResize},
    {"w-resize", Cursor::EwResize},
    {"h-resize", Cursor::EwResize},
    {"col-resize", Cursor::EwResize},
    {"ns-resize", Cursor::NsResize},
    {"n-resize", Cursor::NsResize},
    {"s-resize", Cursor::NsResize},
    {"v-resize", Cursor::NsResize},
    {"row-resize", Cursor::NsResize},
    {"nwse-resize", Cursor::NwseResize},
    {"nw-resize", Cursor::NwseResize},
    {"se-resize", Cursor::NwseResize},
    {"nesw-resize", Cursor::NeswResize},
    {"ne-resize", Cursor::NeswResize},
    {"sw-resize", Cursor::NeswResize},
    {"none", Cursor::None},
    {"disappear", Cursor::None},
    {"wait", Cursor::Wait},
    {"progress", Cursor::Progress},
    {"help", Cursor::Help},
    {"grab", Cursor::Grab},
    {"open-hand", Cursor::Grab},
    {"grabbing", Cursor::Grabbing},
    {"closed-hand", Cursor::Grabbing},
    {"zoom-in", Cursor::ZoomIn},
    {"zoom-out", Cursor::ZoomOut},
    {"context-menu", Cursor::ContextMenu},
    {"alias", Cursor::Alias},
    {"copy", Cursor::Copy},
};

const Cursor* LookupCursor(const std::string& token) {
    for (const CursorKeyword& keyword : kCursors) {
        if (token == keyword.name) {
            return &keyword.cursor;
        }
    }
    return nullptr;
}

}  // namespace

bool parseCursor(std::string_view text, Cursor& cursor) {
    // The first keyword the platform understands wins, so a url() falls through.
    for (const std::string& part : SplitDepth(text, ',')) {
        std::string token = lowerCopy(trimCopy(part));
        if (token.empty() || token.rfind("url(", 0) == 0) {
            continue;
        }
        for (char& ch : token) {
            if (ch == '_') {
                ch = '-';
            }
        }
        const std::size_t space = token.find_first_of(" \t");
        if (space != std::string::npos) {
            token = token.substr(0, space);
        }
        if (const Cursor* found = LookupCursor(token)) {
            cursor = *found;
            return true;
        }
    }
    return false;
}

std::vector<Declaration> parseInlineDeclarations(const std::string& css) { return ParseDeclarations(css); }

void applyDeclarations(ComputedStyle& style, const std::vector<const Declaration*>& declarations, StylePass pass,
                       float inheritedFontSize, float emFontSize) {
    // The web's accent-color, caret-color, and outline-color are read by
    // controls as custom properties (PropertyId::Custom), so they inherit like one.
    if (pass == StylePass::Variables) {
        // The inherited set is shared; this node copies it once, if it declares any.
        std::shared_ptr<CssVariables> own;
        std::vector<std::string> declared;
        for (const Declaration* each : declarations) {
            const Declaration& declaration = *each;
            const std::string& property = declaration.property;
            if (declaration.id != PropertyId::Custom) {
                continue;
            }
            if (!own) {
                own = style.variables ? std::make_shared<CssVariables>(*style.variables) : std::make_shared<CssVariables>();
            }
            const std::string name = property[0] == '-' ? property : "--" + property;
            (*own)[name] = declaration.value;
            declared.push_back(name);
        }
        if (!own) {
            return;
        }
        // As in CSS, a var() in a custom property is resolved on the node that
        // declares it, and its children inherit the result. The inherited ones
        // were resolved on their own nodes already.
        const CssVariables raw = *own;
        for (const std::string& name : declared) {
            std::string& value = (*own)[name];
            if (value.find("var(") != std::string::npos) {
                value = resolveCssVariables(value, &raw);
            }
        }
        style.variables = std::move(own);
        return;
    }
    for (const Declaration* each : declarations) {
        const Declaration& declaration = *each;
        const PropertyId id = declaration.id;
        if (id == PropertyId::Custom) {
            continue;
        }
        const bool fontProperty = id == PropertyId::FontSize || id == PropertyId::FontFamily;
        if ((pass == StylePass::Fonts) != fontProperty) {
            continue;
        }
        // A var() is resolved now, against the custom properties this node has.
        std::string resolvedValue;
        if (declaration.hasVar) {
            resolvedValue = resolveCssVariables(declaration.value, style.variables.get());
            if (resolvedValue.empty()) {
                continue;
            }
        }
        const std::string& value = declaration.hasVar ? resolvedValue : declaration.value;
        switch (id) {
            case PropertyId::FontSize: {
                const ParsedLength length = ParseLength(value);
                if (length.ok) {
                    style.fontSize = static_cast<float>(length.px + (length.em + length.percent) * inheritedFontSize);
                }
                break;
            }
            case PropertyId::FontFamily: {
                std::string family = value;
                const std::size_t comma = family.find(',');
                if (comma != std::string::npos) {
                    family = family.substr(0, comma);
                }
                family = trimCopy(family);
                if (family.size() >= 2 && ((family.front() == '"' && family.back() == '"') ||
                                           (family.front() == '\'' && family.back() == '\''))) {
                    family = family.substr(1, family.size() - 2);
                }
                if (!family.empty()) {
                    style.fontFamily = family;
                }
                break;
            }
            case PropertyId::BackgroundColor: {
                Color color;
                if (!ColorOf(declaration, value, color)) {
                    break;
                }
                style.background.color = color;
                style.background.hasColor = true;
                style.background.visible = true;
                break;
            }
            case PropertyId::Background: {
                Color color;
                if (declaration.parsed.kind == DeclarationValue::Kind::Raw &&
                    lowerCopy(value).find("gradient") != std::string::npos) {
                    style.background.hasColor = false;
                    style.background.color = Color::transparent();
                    ApplyBackgroundImage(style, value);
                    break;
                }
                if (!ColorOf(declaration, value, color)) {
                    break;
                }
                style.background.color = color;
                style.background.hasColor = true;
                style.background.gradient = false;
                style.background.stopCount = 0;
                style.background.visible = color.a > 0.f || style.background.gradient;
                break;
            }
            case PropertyId::BackgroundImage:
                if (lowerCopy(trimCopy(value)) == "none") {
                    style.background.gradient = false;
                    style.background.stopCount = 0;
                    style.background.visible = style.background.hasColor && style.background.color.a > 0.f;
                    break;
                }
                ApplyBackgroundImage(style, value);
                break;
            case PropertyId::FontSmoothing: {
                // subpixel-antialiased keeps stripe coverage. antialiased, none, and grayscale do not.
                // auto is the platform's own choice.
                const std::string mode = lowerCopy(trimCopy(value));
                if (mode == "auto") {
                    style.subpixel = kSubpixelByDefault;
                } else if (mode == "subpixel-antialiased") {
                    style.subpixel = true;
                } else if (mode == "antialiased" || mode == "none" || mode == "grayscale") {
                    style.subpixel = false;
                }
                break;
            }
            case PropertyId::Color: {
                Color color;
                if (ColorOf(declaration, value, color)) {
                    style.color = color;
                }
                break;
            }
            case PropertyId::ImageColor: {
                const std::string mode = lowerCopy(trimCopy(value));
                if (mode == "none") {
                    style.imageColorSet = false;
                    style.imageColorCurrent = false;
                } else if (mode == "currentcolor") {
                    style.imageColorSet = true;
                    style.imageColorCurrent = true;
                } else {
                    bool ok = false;
                    const Color color = Color::parse(value, &ok);
                    if (ok) {
                        style.imageColorSet = true;
                        style.imageColorCurrent = false;
                        style.imageColor = color;
                    }
                }
                break;
            }
            case PropertyId::Width:
            case PropertyId::Height:
            case PropertyId::MinWidth:
            case PropertyId::MinHeight:
            case PropertyId::MaxWidth:
            case PropertyId::MaxHeight: {
                SizeSpec size;
                if (!SizeOf(declaration, value, size)) {
                    break;
                }
                SizeSpec* slot = id == PropertyId::Width       ? &style.width
                                 : id == PropertyId::Height    ? &style.height
                                 : id == PropertyId::MinWidth  ? &style.minWidth
                                 : id == PropertyId::MinHeight ? &style.minHeight
                                 : id == PropertyId::MaxWidth  ? &style.maxWidth
                                                               : &style.maxHeight;
                *slot = size;
                break;
            }
            case PropertyId::BorderRadius: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    AssignRadius(style, lengths, count);
                }
                break;
            }
            case PropertyId::BorderWidth: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    style.border = BoxFromLengths(lengths, count, emFontSize);
                }
                break;
            }
            case PropertyId::BorderColor: {
                Color color;
                if (ColorOf(declaration, value, color)) {
                    style.borderColor = color;
                }
                break;
            }
            case PropertyId::BorderStyle: {
                const std::string kind = lowerCopy(trimCopy(value));
                style.borderStyle = kind == "solid" ? BorderStyle::Solid : BorderStyle::None;
                break;
            }
            case PropertyId::BoxShadow:
                style.shadows.clear();
                if (lowerCopy(trimCopy(value)) == "none") {
                    break;
                }
                for (const std::string& part : SplitDepth(value, ',')) {
                    if (!trimCopy(part).empty()) {
                        style.shadows.push_back(ParseShadow(part, emFontSize));
                    }
                }
                break;
            case PropertyId::Padding: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    style.padding = BoxFromLengths(lengths, count, emFontSize);
                }
                break;
            }
            case PropertyId::Spacing: {
                const ParsedLength length = ParseLength(value);
                if (length.ok && length.percent == 0.0) {
                    style.spacing = static_cast<float>(ResolveLength(length, emFontSize));
                }
                break;
            }
            case PropertyId::Gap:
            case PropertyId::RowGap:
            case PropertyId::ColumnGap: {
                // As in CSS: gap is row-gap then column-gap, and one value sets both.
                // A box's spacing follows gap too.
                std::vector<float> lengths;
                for (const std::string& part : SplitDepth(value, ' ')) {
                    const ParsedLength length = ParseLength(trimCopy(part));
                    if (length.ok && length.percent == 0.0) {
                        lengths.push_back(static_cast<float>(ResolveLength(length, emFontSize)));
                    }
                }
                if (lengths.empty()) {
                    break;
                }
                if (id == PropertyId::RowGap) {
                    style.rowGap = lengths.front();
                } else if (id == PropertyId::ColumnGap) {
                    style.columnGap = lengths.front();
                } else {
                    style.rowGap = lengths.front();
                    style.columnGap = lengths.size() > 1 ? lengths[1] : lengths.front();
                    style.spacing = lengths.front();
                }
                break;
            }
            case PropertyId::Alignment:
                style.alignment = ParseAlignment(value);
                style.alignmentFromCss = true;
                break;
            case PropertyId::Orientation: {
                const std::string kind = lowerCopy(trimCopy(value));
                if (kind == "horizontal" || kind == "vertical") {
                    style.orientationFromCss = true;
                    style.orientation = kind == "vertical" ? Orientation::Vertical : Orientation::Horizontal;
                }
                break;
            }
            case PropertyId::Opacity: {
                double opacity = 1;
                std::size_t consumed = 0;
                if (ParseNumber(value, opacity, consumed)) {
                    style.opacity = static_cast<float>(std::clamp(opacity, 0.0, 1.0));
                }
                break;
            }
            case PropertyId::IndeterminateBarLength: {
                const ParsedLength length = ParseLength(value);
                if (length.ok && length.percent == 0.0) {
                    style.indeterminateBarLengthSet = true;
                    style.indeterminateBarLength = SizeSpec::px(ResolveLength(length, emFontSize));
                }
                break;
            }
            case PropertyId::IndeterminateBarEscape:
            case PropertyId::IndeterminateBarFlip: {
                const std::string kind = lowerCopy(trimCopy(value));
                if (kind == "true" || kind == "false") {
                    const bool enabled = kind == "true";
                    if (id == PropertyId::IndeterminateBarEscape) {
                        style.indeterminateBarEscapeSet = true;
                        style.indeterminateBarEscape = enabled;
                    } else {
                        style.indeterminateBarFlipSet = true;
                        style.indeterminateBarFlip = enabled;
                    }
                }
                break;
            }
            case PropertyId::IndeterminateBarAnimationTime: {
                double seconds = 0;
                std::size_t consumed = 0;
                if (ParseNumber(value, seconds, consumed) && consumed == trimCopy(value).size()) {
                    style.indeterminateBarAnimationTimeSet = true;
                    style.indeterminateBarAnimationTime = std::max(0.0, seconds);
                }
                break;
            }
            case PropertyId::Transition:
                ApplyTransition(style, value);
                break;
            default:
                break;
        }
    }
}
}  // namespace jadefx
