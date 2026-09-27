#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/text/Font.hpp"

#include <memory>
#include <string>

namespace jadefx {

// Where a Labeled puts its graphic against its text, as in JavaFX.
// Center layers the text over the graphic.
enum class ContentDisplay { Left, Right, Top, Bottom, Center, TextOnly, GraphicOnly };

// Text with an optional graphic node, in the shape of OpenJFX Labeled.
// The graphic sits beside the text by contentDisplay, graphicTextGap apart, and
// alignment places the two together in the content box. A line wider than the
// room left for text ends in an ellipsis.
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

    // Null clears it. A node in another parent moves here.
    void setGraphic(std::shared_ptr<Node> graphic);
    const std::shared_ptr<Node>& getGraphic() const { return graphic_; }
    void setContentDisplay(ContentDisplay display) { contentDisplay_ = display; }
    ContentDisplay getContentDisplay() const { return contentDisplay_; }
    // Points between the graphic and the text. The default is 4.
    void setGraphicTextGap(double gap) { graphicTextGap_ = gap; }
    double getGraphicTextGap() const { return graphicTextGap_; }

    void detachChild(Node* child) override;

protected:
    explicit Labeled(std::string text);
    void renderContent(UiRenderer& renderer, float opacity) override;
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct Block;

    bool showsText() const;
    bool showsGraphic() const;
    // Where the graphic and the text go inside the content box, local to this node.
    Block arrange() const;

    std::string text_;
    std::shared_ptr<Node> graphic_;
    ContentDisplay contentDisplay_ = ContentDisplay::Left;
    double graphicTextGap_ = 4;
};

}  // namespace jadefx
