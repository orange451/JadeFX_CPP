#pragma once

#include "jadefx/scene/Controls/StyleClassedTextArea.hpp"

namespace jadefx {

// A code editor: line numbers, the current paragraph highlighted, tabs as spaces,
// and folds. Wrap is off so columns line up. The face is whatever font the area inherits.
class CodeArea : public StyleClassedTextArea {
public:
    CodeArea();

    const char* getElementType() const override { return "codearea"; }
};

}  // namespace jadefx
