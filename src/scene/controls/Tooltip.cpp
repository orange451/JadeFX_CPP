#include "jadefx/scene/controls/Tooltip.hpp"

namespace jadefx {
namespace {

Tooltip* tooltipContent(const HoverPopup* popup) {
    if (popup == nullptr || !popup->content) {
        return nullptr;
    }
    return dynamic_cast<Tooltip*>(popup->content.get());
}

}  // namespace

Tooltip::Tooltip() : Tooltip("") {}

Tooltip::Tooltip(std::string text) : Labeled(std::move(text)) {
    setTextFill(Color::white());
    setBackground(Color::rgb8(60, 64, 67));
    setPadding(Insets::axes(6, 8));
    setAlignment(Pos::Center);
    setStyle("border-radius: 4px;");
}

Tooltip::~Tooltip() {
    Node* host = host_;
    host_ = nullptr;
    if (host == nullptr) {
        return;
    }
    const HoverPopup* popup = host->getHoverPopup();
    // The host popup owns this object. A zero use count means that shared_ptr is
    // already running this destructor, so clearing it again would re-enter reset.
    if (popup == nullptr || popup->content.get() != this || popup->content.use_count() == 0) {
        return;
    }
    host->clearHoverPopup();
}

void Tooltip::setShowDelay(double seconds) {
    showDelay_ = seconds;
    publishInstalled();
}

void Tooltip::setHideDelay(double seconds) {
    hideDelay_ = seconds;
    publishInstalled();
}

void Tooltip::setShowDuration(double seconds) {
    showDuration_ = seconds;
    publishInstalled();
}

void Tooltip::publish(std::shared_ptr<Node> content) {
    if (host_ == nullptr || !content) {
        return;
    }
    HoverPopup hover;
    hover.content = std::move(content);
    hover.showDelay = showDelay_;
    hover.hideDelay = hideDelay_;
    hover.showDuration = showDuration_;
    host_->setHoverPopup(std::move(hover));
}

void Tooltip::publishInstalled() {
    if (host_ == nullptr) {
        return;
    }
    const HoverPopup* existing = host_->getHoverPopup();
    if (existing == nullptr || existing->content.get() != this) {
        return;
    }
    publish(existing->content);
}

void Tooltip::install(Node* node, const std::shared_ptr<Tooltip>& tooltip) {
    if (node == nullptr) {
        return;
    }
    if (!tooltip) {
        uninstall(node);
        return;
    }
    if (tooltip->host_ != nullptr && tooltip->host_ != node) {
        Node* previous = tooltip->host_;
        const HoverPopup* popup = previous->getHoverPopup();
        const bool owned = popup != nullptr && popup->content.get() == tooltip.get();
        tooltip->host_ = nullptr;
        if (owned) {
            previous->clearHoverPopup();
        }
    }
    if (const HoverPopup* existing = node->getHoverPopup()) {
        Tooltip* other = tooltipContent(existing);
        if (other != nullptr && other != tooltip.get() && other->host_ == node) {
            other->host_ = nullptr;
        }
    }
    tooltip->host_ = node;
    tooltip->publish(tooltip);
}

void Tooltip::uninstall(Node* node) {
    if (node == nullptr) {
        return;
    }
    if (Tooltip* tip = tooltipContent(node->getHoverPopup())) {
        if (tip->host_ == node) {
            tip->host_ = nullptr;
        }
    }
    node->clearHoverPopup();
}

void Tooltip::renderContent(UiRenderer& renderer, float opacity) {
    Labeled::renderContent(renderer, opacity);
}

}  // namespace jadefx
