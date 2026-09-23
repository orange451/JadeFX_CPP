#pragma once

#include "jadefx/scene/layout/Region.hpp"

namespace jadefx {

// Publishes the child list.
class Pane : public Region {
public:
    Pane() = default;

    const char* getElementType() const override { return "pane"; }

    ObservableList<std::shared_ptr<Node>>& getChildren() { return children(); }
    const ObservableList<std::shared_ptr<Node>>& getChildren() const { return children(); }

protected:
    // Children keep their measured size and sit at the top-left of the content box.
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
};

}  // namespace jadefx
