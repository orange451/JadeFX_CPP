#pragma once

#include "jadefx/scene/Controls/Labeled.hpp"

#include <string>

namespace jadefx {

class Label : public Labeled {
public:
    Label();
    explicit Label(std::string text);

    // Stripe coverage is on unless this or font-smoothing turns it off.
    void setSubpixelRendering(bool enabled);
    bool isSubpixelRendering() const { return computedStyle().subpixel; }

    const char* getElementType() const override { return "label"; }
};

}  // namespace jadefx
