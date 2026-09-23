#pragma once

#include "jadefx/scene/Parent.hpp"

namespace jadefx {

// Padding and border insets around the content box.
class Region : public Parent {
public:
    void setBorderWidth(float width) { setBorder(Insets::uniform(width)); }
    float getBorderWidth() const;

protected:
    Region() = default;
};

}  // namespace jadefx
