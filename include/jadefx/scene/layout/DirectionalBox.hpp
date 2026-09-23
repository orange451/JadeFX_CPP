#pragma once

#include "jadefx/scene/layout/Spacable.hpp"
#include "jadefx/scene/layout/StackPane.hpp"

namespace jadefx {

// Shared by VBox and HBox. Spacing is the gap between children.
class DirectionalBox : public StackPane, public Spacable {
public:
    void setSpacing(double spacing) override;
    double getSpacing() const override;

protected:
    DirectionalBox();
};

}  // namespace jadefx
