#include "jadefx/scene/SubScene.hpp"

#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <utility>

namespace jadefx {

SubScene::SubScene(std::shared_ptr<Node> root) {
    if (root) {
        setRoot(std::move(root));
    }
}

void SubScene::setRoot(std::shared_ptr<Node> root) {
    if (root_ == root) {
        return;
    }
    if (root_) {
        detachChild(root_.get());
    }
    if (root) {
        root_ = root;
        children().add(std::move(root));
        // After the add, since leaving another SubScene takes the state away.
        root_->setPseudoState("root", true);
    }
}

void SubScene::detachChild(Node* child) {
    if (child != nullptr && child == root_.get()) {
        // Held until the list lets go, so the node is alive while it leaves.
        const std::shared_ptr<Node> leaving = std::move(root_);
        leaving->setPseudoState("root", false);
        Node::detachChild(child);
        return;
    }
    Node::detachChild(child);
}

void SubScene::setUserAgentStylesheet(std::string cssOrTheme) {
    userAgent_ = cssOrTheme.empty() ? Stylesheet() : Stylesheet::parse(Theme::expand(cssOrTheme));
    userAgentSource_ = std::move(cssOrTheme);
}

const Stylesheet& SubScene::userAgentStylesheet() const {
    if (!userAgentSource_.empty()) {
        return userAgent_;
    }
    return getScene() != nullptr ? getScene()->userAgentStylesheet() : Theme::userAgentStylesheet();
}

void SubScene::layoutChildren() {
    if (root_) {
        root_->performLayout(contentLeft(), contentTop(), contentWidth(), contentHeight());
    }
}

double SubScene::preferredContentWidth(double innerAvailable) const {
    return root_ ? root_->measuredWidth(innerAvailable) : 0.0;
}

double SubScene::preferredContentHeight(double innerWidth) const {
    return root_ ? root_->measuredHeight(innerWidth, -1) : 0.0;
}

void SubScene::renderChildren(UiRenderer& renderer, float opacity) {
    renderer.pushClip(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()), static_cast<float>(getWidth()),
                      static_cast<float>(getHeight()));
    Node::renderChildren(renderer, opacity);
    renderer.popClip();
}

}  // namespace jadefx
