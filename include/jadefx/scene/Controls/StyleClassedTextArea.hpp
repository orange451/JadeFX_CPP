#pragma once

#include "jadefx/scene/Controls/StyledTextArea.hpp"

#include <string>
#include <unordered_map>

namespace jadefx {

// Styles are CSS class names, resolved through defineStyleClass.
// A span may list several names separated by spaces. Explicit colors on the span win.
class StyleClassedTextArea : public StyledTextArea {
public:
    const char* getElementType() const override { return "styleclassedtextarea"; }

    void defineStyleClass(std::string name, TextStyle style);
    void setStyleClass(int start, int end, std::string className);

protected:
    TextStyle resolveStyle(const TextStyle& style) const override;

private:
    std::unordered_map<std::string, TextStyle> classes_;
};

}  // namespace jadefx
