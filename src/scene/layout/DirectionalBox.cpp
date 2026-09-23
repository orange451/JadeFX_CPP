#include "jadefx/scene/layout/DirectionalBox.hpp"

namespace jadefx {

DirectionalBox::DirectionalBox() { setAlignment(Pos::TopLeft); }

void DirectionalBox::setSpacing(double spacing) { setSpacingValue(spacing); }

double DirectionalBox::getSpacing() const { return spacingValue(); }

}  // namespace jadefx
