#pragma once

#include "jadefx/scene/layout/StackPane.hpp"

#include <functional>
#include <memory>

namespace jadefx {

// Top, bottom, left, right, and center regions. Children are placed with the
// setters below. Top and bottom span the width, left and right fill the
// leftover height, and center takes what remains. An explicit size or maximum
// keeps a region smaller than that slot; minimum size is kept. setSpacing
// inserts a gap between top or bottom and the middle, and between left or
// right and the center.
class BorderPane : public StackPane {
public:
    BorderPane();
    ~BorderPane() override;

    const char* getElementType() const override { return "borderpane"; }

    void setSpacing(double spacing);
    double getSpacing() const { return regionSpacing_; }

    void setCenter(std::shared_ptr<Node> node);
    void setTop(std::shared_ptr<Node> node);
    void setBottom(std::shared_ptr<Node> node);
    void setLeft(std::shared_ptr<Node> node);
    void setRight(std::shared_ptr<Node> node);

    Node* getCenter() const { return center_.get(); }
    Node* getTop() const { return top_.get(); }
    Node* getBottom() const { return bottom_.get(); }
    Node* getLeft() const { return left_.get(); }
    Node* getRight() const { return right_.get(); }

protected:
    void layoutChildren() override;
    void visitChildren(const std::function<void(Node*)>& visitor) override;
    void detachChild(Node* child) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    void adopt(std::shared_ptr<Node>& slot, std::shared_ptr<Node> node);
    void releaseSlot(std::shared_ptr<Node>& slot, Node* child);

    std::shared_ptr<Node> center_;
    std::shared_ptr<Node> top_;
    std::shared_ptr<Node> bottom_;
    std::shared_ptr<Node> left_;
    std::shared_ptr<Node> right_;
    double regionSpacing_ = 0;
};

}  // namespace jadefx
