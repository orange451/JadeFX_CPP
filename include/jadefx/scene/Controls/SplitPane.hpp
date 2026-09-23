#pragma once

#include "jadefx/scene/Controls/Controls.hpp"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

namespace jadefx {

// Two or more nodes with a draggable divider between each pair, in the shape of
// JavaFX SplitPane. Horizontal places the items in a row. Vertical stacks them.
// A divider position is a fraction from 0 to 1. The pane keeps each divider
// inside the neighboring items' minimum and maximum sizes, so the fraction read
// back can differ from the fraction that was set. Dragging a divider writes the
// fraction back. setResizableWithParent(node, false) keeps that item's size when
// the pane itself is resized.
//
// The control type is split-pane. Dividers are split-pane-divider. :horizontal
// and :vertical follow the orientation. The grip is horizontal-grabber or
// vertical-grabber. Divider thickness is the divider's left padding plus its
// right padding, for both orientations.
class SplitPane : public Controls {
public:
    // One divider. Position defaults to 0.5. The list is rebuilt when items change,
    // so a Divider taken from getDividers() is only valid until the next item edit.
    class Divider {
    public:
        void setPosition(double value);
        double getPosition() const { return position_; }

    private:
        friend class SplitPane;
        double position_ = 0.5;
        SplitPane* owner_ = nullptr;
    };

    SplitPane();
    ~SplitPane() override;

    const char* getElementType() const override { return "split-pane"; }

    ObservableList<std::shared_ptr<Node>>& getItems();
    const ObservableList<std::shared_ptr<Node>>& getItems() const;

    // Live divider objects. Call setPosition on them, and leave the list's length alone.
    std::vector<Divider>& getDividers();
    const std::vector<Divider>& getDividers() const;

    // Index past the last divider is remembered and applied when that divider exists.
    // A negative index is ignored.
    void setDividerPosition(int dividerIndex, double position);
    void setDividerPositions(std::initializer_list<double> positions);
    std::vector<double> getDividerPositions() const;

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return orientation_; }

    // nullopt removes the flag, which means the item resizes with the pane.
    static void setResizableWithParent(Node& node, std::optional<bool> value);
    static bool isResizableWithParent(const Node& node);

protected:
    void layoutChildren() override;
    void styleDidApply() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct Impl;
    class ContentHost;
    class DividerHost;

    void onAdded(std::shared_ptr<Node> item, std::size_t index);
    void onRemoved(std::shared_ptr<Node> item, std::size_t index);
    void forgetItem(Node* item);
    void insertBand(std::size_t index, const std::shared_ptr<Node>& item);
    void removeBand(std::size_t index);
    void relayoutFromItems(int from, int removedCount);
    void rebuildDividers();
    void syncChrome();
    void pressDivider(std::size_t index, const MouseEvent& event);
    void dragDivider(std::size_t index, const MouseEvent& event);
    void onDividerPositionChanged(Divider* divider);
    double contentSpan() const;
    bool horizontalSplit() const { return orientation_ == Orientation::Horizontal; }

    std::unique_ptr<Impl> impl_;
    Orientation orientation_ = Orientation::Horizontal;
};

}  // namespace jadefx
