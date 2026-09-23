#pragma once

#include "jadefx/scene/layout/DirectionalBox.hpp"

namespace jadefx {

class HBox : public DirectionalBox {
public:
    HBox() = default;

    const char* getElementType() const override { return "hbox"; }

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
};

}  // namespace jadefx
