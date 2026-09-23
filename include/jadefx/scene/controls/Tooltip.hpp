#pragma once

#include "jadefx/scene/controls/Labeled.hpp"

#include <memory>
#include <string>

namespace jadefx {

// Text shown beside a node after the pointer rests on it.
// Scene owns the timing. This node is the popup content.
class Tooltip : public Labeled {
public:
    Tooltip();
    explicit Tooltip(std::string text);
    ~Tooltip() override;

    const char* getElementType() const override { return "tooltip"; }

    void setShowDelay(double seconds);
    double getShowDelay() const { return showDelay_; }
    void setHideDelay(double seconds);
    double getHideDelay() const { return hideDelay_; }
    void setShowDuration(double seconds);
    double getShowDuration() const { return showDuration_; }

    // Attaches this tooltip to node. Replaces any previous hover popup on that node.
    // Installing on a new node detaches it from the previous one.
    // Passing a null tooltip clears the node. A null node is ignored.
    static void install(Node* node, const std::shared_ptr<Tooltip>& tooltip);
    static void uninstall(Node* node);

protected:
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    void publish(std::shared_ptr<Node> content);
    void publishInstalled();

    double showDelay_ = 1;
    double hideDelay_ = 0.2;
    double showDuration_ = 5;
    // Raw host, not a shared_ptr: the host's hover popup already owns this tooltip.
    Node* host_ = nullptr;
};

}  // namespace jadefx
