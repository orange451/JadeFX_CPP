#pragma once

#include "stb_truetype.h"

#include <memory>
#include <string>
#include <vector>

namespace jadefx {

// The platform rasterizer's handle to the same font, made on first use.
struct NativeFace;

struct FontFace {
    std::string family;
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool ready = false;
    mutable std::shared_ptr<NativeFace> native;
};

const FontFace* LookupFace(const std::string& family);

}  // namespace jadefx
