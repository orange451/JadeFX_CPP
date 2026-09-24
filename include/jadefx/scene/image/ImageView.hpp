#pragma once

#include "jadefx/scene/Node.hpp"
#include "jadefx/scene/image/Image.hpp"

#include <memory>

namespace jadefx {

// Draws an Image in this node's box. The preferred size is the bitmap size in
// points. The bitmap stretches to the laid-out box.
class ImageView : public Node {
public:
    ImageView();
    explicit ImageView(std::shared_ptr<Image> image);

    void setImage(std::shared_ptr<Image> image);
    std::shared_ptr<Image> getImage() const { return image_; }

    const char* getElementType() const override { return "image-view"; }

protected:
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    std::shared_ptr<Image> image_;
};

}  // namespace jadefx
