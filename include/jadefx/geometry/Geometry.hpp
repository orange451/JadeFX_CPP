#pragma once

namespace jadefx {

// Horizontal and vertical alignment, matching JavaFX / JadeFX Pos.
enum class HPos { Left, Center, Right };
enum class VPos { Top, Center, Bottom };

enum class Pos {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    // Use the first ancestor that has a real alignment. The root falls back to center.
    Ancestor
};

// Which edge a strip attaches to. TabPane puts its headers there.
enum class Side { Top, Bottom, Left, Right };

enum class Orientation { Horizontal, Vertical };

enum class ScreenOrientation { Portrait, Landscape, All };

struct Size {
    double width = 0;
    double height = 0;
};

struct Insets {
    double top = 0;
    double right = 0;
    double bottom = 0;
    double left = 0;

    static constexpr Insets empty() { return {}; }

    static constexpr Insets uniform(double value) { return {value, value, value, value}; }

    static constexpr Insets axes(double vertical, double horizontal) {
        return {vertical, horizontal, vertical, horizontal};
    }

    constexpr double width() const { return left + right; }
    constexpr double height() const { return top + bottom; }
};

inline HPos hpos(Pos pos) {
    switch (pos) {
        case Pos::TopLeft:
        case Pos::CenterLeft:
        case Pos::BottomLeft:
            return HPos::Left;
        case Pos::TopRight:
        case Pos::CenterRight:
        case Pos::BottomRight:
            return HPos::Right;
        case Pos::TopCenter:
        case Pos::Center:
        case Pos::BottomCenter:
        case Pos::Ancestor:
            return HPos::Center;
    }
    return HPos::Center;
}

inline VPos vpos(Pos pos) {
    switch (pos) {
        case Pos::TopLeft:
        case Pos::TopCenter:
        case Pos::TopRight:
            return VPos::Top;
        case Pos::BottomLeft:
        case Pos::BottomCenter:
        case Pos::BottomRight:
            return VPos::Bottom;
        case Pos::CenterLeft:
        case Pos::Center:
        case Pos::CenterRight:
        case Pos::Ancestor:
            return VPos::Center;
    }
    return VPos::Center;
}

}  // namespace jadefx
