#include "jadefx/scene/Controls/InlineCssTextArea.hpp"

#include "scene/text/TextCss.hpp"

namespace jadefx {

void InlineCssTextArea::setStyle(int start, int end, const std::string& css) {
    StyledTextArea::setStyle(start, end, ParseTextCss(css));
}

}  // namespace jadefx
