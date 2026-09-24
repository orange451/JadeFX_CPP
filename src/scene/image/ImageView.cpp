#include "jadefx/scene/image/ImageView.hpp"

#include "gl/UiRenderer.hpp"

namespace jadefx {

ImageView::ImageView() = default;

ImageView::ImageView(std::shared_ptr<Image> image) : image_(std::move(image)) {}

void ImageView::setImage(std::shared_ptr<Image> image) { image_ = std::move(image); }

double ImageView::preferredContentWidth(double) const {
    return image_ ? static_cast<double>(image_->getWidth()) : 0;
}

double ImageView::preferredContentHeight(double) const {
    return image_ ? static_cast<double>(image_->getHeight()) : 0;
}

void ImageView::renderContent(UiRenderer& renderer, float opacity) {
    if (!image_ || !image_->data_) {
        return;
    }
    renderer.drawImage(image_->data_, static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                       static_cast<float>(getWidth()), static_cast<float>(getHeight()), opacity);
}

}  // namespace jadefx
