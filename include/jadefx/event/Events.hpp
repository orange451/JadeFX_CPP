#pragma once

#include <functional>
#include <string>

namespace jadefx {

class Node;

struct MouseEvent {
    double x = 0;
    double y = 0;
    int button = 0;
    Node* target = nullptr;
};

struct ScrollEvent {
    double x = 0;
    double y = 0;
    double deltaX = 0;
    double deltaY = 0;
    Node* target = nullptr;
    bool consumed = false;

    void consume() { consumed = true; }
};

// Key codes match GLFW, so a desktop host can forward them unchanged.
// Letters and digits use their ASCII values.
namespace Key {
constexpr int Unknown = -1;
constexpr int Space = 32;
constexpr int Apostrophe = 39;
constexpr int Comma = 44;
constexpr int Minus = 45;
constexpr int Period = 46;
constexpr int Slash = 47;
constexpr int Digit0 = 48;
constexpr int Digit9 = 57;
constexpr int Semicolon = 59;
constexpr int Equal = 61;
constexpr int A = 65;
constexpr int B = 66;
constexpr int C = 67;
constexpr int D = 68;
constexpr int I = 73;
constexpr int U = 85;
constexpr int V = 86;
constexpr int X = 88;
constexpr int Y = 89;
constexpr int Z = 90;
constexpr int LeftBracket = 91;
constexpr int Backslash = 92;
constexpr int RightBracket = 93;
constexpr int Escape = 256;
constexpr int Enter = 257;
constexpr int Tab = 258;
constexpr int Backspace = 259;
constexpr int Insert = 260;
constexpr int Delete = 261;
constexpr int Right = 262;
constexpr int Left = 263;
constexpr int Down = 264;
constexpr int Up = 265;
constexpr int PageUp = 266;
constexpr int PageDown = 267;
constexpr int Home = 268;
constexpr int End = 269;
constexpr int KpEnter = 335;
constexpr int LeftShift = 340;
constexpr int LeftControl = 341;
constexpr int LeftAlt = 342;
constexpr int LeftSuper = 343;
constexpr int RightShift = 344;
constexpr int RightControl = 345;
constexpr int RightAlt = 346;
constexpr int RightSuper = 347;

constexpr int ModShift = 0x1;
constexpr int ModControl = 0x2;
constexpr int ModAlt = 0x4;
constexpr int ModSuper = 0x8;
}  // namespace Key

struct KeyEvent {
    int key = 0;
    bool pressed = false;
    bool repeat = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool meta = false;
    bool consumed = false;

    void consume() { consumed = true; }
    // Ctrl on Windows and Linux, Command on macOS. Either one is accepted.
    bool shortcut() const { return control || meta; }
};

struct TextEvent {
    std::string text;
    bool consumed = false;

    void consume() { consumed = true; }
};

using MouseHandler = std::function<void(const MouseEvent&)>;
using ScrollHandler = std::function<void(const ScrollEvent&)>;

}  // namespace jadefx
