#pragma once

#include "jadefx/scene/controls/StyledTextArea.hpp"

#include <string>

namespace jadefx {

// Styles are CSS declarations, for example "color: #c586c0; font-weight: bold;".
// color, background-color, font-weight, font-style, font-size, and text-decoration are read.
// Cmd/Ctrl+B toggles bold and Cmd/Ctrl+U toggles underline. With no selection, the next
// characters use that style.
class InlineCssTextArea : public StyledTextArea {
public:
    const char* getElementType() const override { return "inlinecsstextarea"; }

    void setStyle(int start, int end, const std::string& css);
    void handleKey(KeyEvent& event) override;

private:
    void toggleDecoration(bool underline);
};

}  // namespace jadefx
