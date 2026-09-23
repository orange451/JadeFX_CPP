#pragma once

#include "jadefx/scene/layout/DirectionalBox.hpp"

namespace jadefx {

class VBox : public DirectionalBox {
public:
    VBox() = default;

    const char* getElementType() const override { return "vbox"; }

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
};

}  // namespace jadefx
