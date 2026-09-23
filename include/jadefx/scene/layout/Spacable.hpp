#pragma once

namespace jadefx {

// A gap between children. DirectionalBox implements this for VBox and HBox.
class Spacable {
public:
    virtual ~Spacable() = default;

    virtual void setSpacing(double spacing) = 0;
    virtual double getSpacing() const = 0;

protected:
    Spacable() = default;
};

}  // namespace jadefx
