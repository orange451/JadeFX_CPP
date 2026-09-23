#include "jadefx/scene/controls/Button.hpp"

namespace jadefx {

Button::Button() : ButtonBase("") {}

Button::Button(std::string text) : ButtonBase(std::move(text)) {}

}  // namespace jadefx
