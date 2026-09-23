#include "jadefx/scene/Controls/Label.hpp"

namespace jadefx {

Label::Label() : Labeled("") {}

Label::Label(std::string text) : Labeled(std::move(text)) {}

void Label::setSubpixelRendering(bool enabled) { setSubpixelRenderingInternal(enabled); }

}  // namespace jadefx
