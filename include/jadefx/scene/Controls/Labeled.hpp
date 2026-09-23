#pragma once

#include "jadefx/scene/Controls/Controls.hpp"
#include "jadefx/scene/text/Font.hpp"

#include <string>

namespace jadefx {

class Labeled : public Controls {
public:
    void setText(std::string text);
    const std::string& getText() const { return text_; }
    // The line drawn in the current content box. Wider text keeps a prefix and an ellipsis.
    std::string displayedText() const;

    void setTextFill(const Color& color);
    Color getTextFill() const { return textFill(); }

    void setFont(const Font& font);
    Font getFont() const { return font(); }

protected:
    explicit Labeled(std::string text);
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    std::string text_;
};

}  // namespace jadefx
