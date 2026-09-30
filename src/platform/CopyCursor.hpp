#pragma once

#include <vector>

namespace jadefx {

// RGBA, not premultiplied, row by row from the top.
struct CursorImage {
    int width = 0;
    int height = 0;
    int hotX = 0;
    int hotY = 0;
    std::vector<unsigned char> pixels;
};

// The pointer that says a drop adds a copy, for a system without one: the
// arrow, with a plus in a box below and to its right. The hot spot is the tip.
CursorImage CopyCursorImage();

// Shows the system's drag-copy pointer in place of the one GLFW set, until the
// next glfwSetCursor. False where the system has none, as off macOS.
bool ShowSystemCopyCursor();

}  // namespace jadefx
