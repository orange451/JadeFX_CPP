#include "jadefx/scene/Controls/CodeArea.hpp"

namespace jadefx {

CodeArea::CodeArea() {
    setWrapText(false);
    setShowLineNumbers(true);
    setHighlightCurrentParagraph(true);
    setInsertSpacesForTab(true);
    setAutoIndent(true);
    setTabSize(4);
}

}  // namespace jadefx
