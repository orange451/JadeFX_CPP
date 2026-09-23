#pragma once

#include "jadefx/scene/Controls/StyledTextArea.hpp"

#include <string>

namespace jadefx {

// Styles are CSS declarations, for example "color: #c586c0; font-weight: bold;".
// color, background-color, font-weight, font-style, font-size, and text-decoration are read.
// The JavaFX names -fx-fill, -fx-font-weight, -fx-underline, and -fx-strikethrough are accepted too.
class InlineCssTextArea : public StyledTextArea {
public:
    const char* getElementType() const override { return "inlinecsstextarea"; }

    void setStyle(int start, int end, const std::string& css);
};

}  // namespace jadefx
