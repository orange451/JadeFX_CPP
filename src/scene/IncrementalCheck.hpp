#pragma once

#include "jadefx/scene/Scene.hpp"

#include <string>
#include <vector>

namespace jadefx {

// One node's resolved style and bounds, as a pass left them.
struct NodeSnapshot {
    const Node* node = nullptr;
    std::string path;
    ComputedStyle style;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    // The node asked to be laid out again next frame, as an animation driven by the
    // clock does. A second pass in the same frame may move it, so its subtree's
    // bounds are not compared.
    bool animating = false;
};

// The check behind JADEFX_VERIFY_INCREMENTAL: a full pass over a scene that an
// incremental pass just styled and laid out must change nothing.
class IncrementalCheck {
public:
    // True in a debug build when JADEFX_VERIFY_INCREMENTAL is 1.
    static bool requested();
    // Every node under root, root first, in visitChildren order.
    static std::vector<NodeSnapshot> snapshot(Node& root);
    // Empty when the two agree. Otherwise "<path>: <field>" for the first difference.
    static std::string compare(const std::vector<NodeSnapshot>& incremental, const std::vector<NodeSnapshot>& full);
    // The first ComputedStyle field that differs, or null.
    static const char* firstStyleDifference(const ComputedStyle& a, const ComputedStyle& b);
    // Styles and lays out scene again ignoring dirty flags, and compares with before.
    static std::string verify(Scene& scene);

private:
    static void collect(Node& node, const std::string& path, std::vector<NodeSnapshot>& out);
};

}  // namespace jadefx
