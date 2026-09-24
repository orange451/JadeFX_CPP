#pragma once

#include <vector>

namespace jadefx {

// Straight-alpha RGBA, top row first. UiRenderer uploads this once and keeps
// the texture until the last Image that owns it is gone.
struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

}  // namespace jadefx
