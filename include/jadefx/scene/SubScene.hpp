#pragma once

#include "jadefx/scene/Node.hpp"

#include <memory>
#include <string>

namespace jadefx {

// A part of a scene with a cascade of its own, as JavaFX's SubScene. The nodes
// inside are styled from its user-agent stylesheet instead of the scene's;
// stylesheets on the SubScene and its ancestors do not reach them; and its
// root inherits no text color, font, cursor, or custom property from outside.
// The root matches :root. The SubScene itself is styled by the outer cascade,
// as any node. Input, focus, and popups stay with the window's Scene, and
// getScene() inside returns it.
//
// The root is laid out over the SubScene's content box, and drawing inside is
// clipped to the SubScene's bounds.
class SubScene : public Node {
public:
    explicit SubScene(std::shared_ptr<Node> root = nullptr);

    const char* getElementType() const override { return "subscene"; }

    void setRoot(std::shared_ptr<Node> root);
    Node* getRoot() const { return root_.get(); }

    // The lowest layer of the cascade inside: light, dark, or CSS text, as
    // Scene::setUserAgentStylesheet takes. Empty uses the scene's. Switching
    // it restyles the nodes inside at the next layout.
    void setUserAgentStylesheet(std::string cssOrTheme);
    const std::string& getUserAgentStylesheet() const { return userAgentSource_; }
    // The user-agent stylesheet the nodes inside are styled with.
    const Stylesheet& userAgentStylesheet() const;

    void detachChild(Node* child) override;

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void renderChildren(UiRenderer& renderer, float opacity) override;
    const SubScene* asSubScene() const override { return this; }

private:
    std::shared_ptr<Node> root_;
    std::string userAgentSource_;
    Stylesheet userAgent_;
};

}  // namespace jadefx
