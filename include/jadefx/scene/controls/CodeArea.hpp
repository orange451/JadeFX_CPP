#pragma once

#include "jadefx/scene/controls/StyleClassedTextArea.hpp"

namespace jadefx {

// A code editor: line numbers, the current paragraph highlighted, tabs as spaces,
// and folds. Wrap is off so columns line up. The face is whatever font the area inherits.
// Paste inserts characters. Style classes come from the caller, usually a highlighter.
class CodeArea : public StyleClassedTextArea {
public:
    CodeArea();

    const char* getElementType() const override { return "codearea"; }

protected:
    bool pasteKeepsStyle() const override { return false; }
};

}  // namespace jadefx
