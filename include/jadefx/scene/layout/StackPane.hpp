#pragma once

#include "jadefx/scene/layout/Pane.hpp"

#include <memory>

namespace jadefx {

class StackPane : public Pane {
public:
    StackPane();
    explicit StackPane(std::shared_ptr<Node> child);

    const char* getElementType() const override { return "stackpane"; }

protected:
    void layoutChildren() override;
};

}  // namespace jadefx
