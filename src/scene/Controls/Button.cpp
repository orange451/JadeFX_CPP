#include "jadefx/scene/Controls/Button.hpp"

namespace jadefx {

Button::Button() : ButtonBase("") {}

Button::Button(std::string text) : ButtonBase(std::move(text)) {}

}  // namespace jadefx
