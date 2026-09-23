#include "jadefx/scene/layout/Region.hpp"

namespace jadefx {

float Region::getBorderWidth() const {
    const Insets insets = getBorder();
    return static_cast<float>((insets.left + insets.right + insets.top + insets.bottom) / 4.0);
}

}  // namespace jadefx
