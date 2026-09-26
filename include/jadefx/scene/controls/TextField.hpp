#pragma once

#include "jadefx/scene/controls/Controls.hpp"

#include <string>

namespace jadefx {

// A single-line editor. Enter fires the action and does not insert a break.
// Up, Down, Escape, and Tab are left unconsumed for a parent such as a combo box.
class TextField : public Controls {
public:
    TextField();
    explicit TextField(std::string text);

    const char* getElementType() const override { return "textfield"; }

    void setText(std::string text);
    const std::string& getText() const { return text_; }
    void setPromptText(std::string text) { prompt_ = std::move(text); }
    const std::string& getPromptText() const { return prompt_; }
    void setEditable(bool editable) { editable_ = editable; }
    bool isEditable() const { return editable_; }

    void setPrefColumnCount(int columns) { columns_ = columns; }
    int getPrefColumnCount() const { return columns_; }

    // Indexes are Unicode code points, clamped to 0..length.
    int getLength() const;
    int getCaretPosition() const { return caret_; }
    int getAnchor() const { return anchor_; }
    void positionCaret(int index);
    void selectRange(int anchor, int caret);
    void selectAll();
    void deselect();
    void clear();
    std::string getSelectedText() const;
    void replaceSelection(std::string text);
    void cut();
    void copy();
    void paste();

    void setOnAction(ActionHandler handler) { onAction_ = std::move(handler); }
    void fire();

    // Window points of the caret bar. False until the field has been given a size.
    bool caretBounds(double& x, double& y, double& height);

protected:
    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseDragged(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void handleText(TextEvent& event) override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct LineLayout;

    Font face() const;
    LineLayout measureLine() const;
    void ensureCaretVisible();
    int indexAt(double absoluteX);
    int characterAt(double absoluteX);
    void moveCaret(int direction, bool extend, bool byWord);
    void moveTo(int index, bool extend);
    void eraseOne(bool forward);
    std::string readClipboard() const;
    void writeClipboard(std::string text);

    std::string text_;
    std::string prompt_;
    // Used only when the field is not in a scene. A scene clipboard wins.
    std::string clipboard_;
    ActionHandler onAction_;
    int columns_ = 12;
    int caret_ = 0;
    int anchor_ = 0;
    float scroll_ = 0.f;
    bool editable_ = true;
    // Presses close in time and place count up to a triple click.
    int clickCount_ = 0;
    double lastPressSeconds_ = 0;
    double lastPressX_ = 0;
    double lastPressY_ = 0;
    // A double-click drag grows by whole words from the word it started on.
    bool dragWords_ = false;
    int wordStart_ = 0;
    int wordEnd_ = 0;
};

}  // namespace jadefx
