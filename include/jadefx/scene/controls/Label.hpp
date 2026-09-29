#pragma once

#include "jadefx/scene/controls/Labeled.hpp"

#include <string>

namespace jadefx {

class Label : public Labeled {
public:
    Label();
    explicit Label(std::string text);

    // Stripe coverage follows kSubpixelByDefault unless this or font-smoothing sets it.
    void setSubpixelRendering(bool enabled);
    bool isSubpixelRendering() const { return computedStyle().subpixel; }

    const char* getElementType() const override { return "label"; }
};

}  // namespace jadefx
