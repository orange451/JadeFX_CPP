#pragma once

#include "stb_truetype.h"

#include <string>
#include <vector>

namespace jadefx {

struct FontFace {
    std::string family;
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool ready = false;
};

const FontFace* LookupFace(const std::string& family);

}  // namespace jadefx
