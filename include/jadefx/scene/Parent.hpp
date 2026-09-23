#pragma once

#include "jadefx/scene/Node.hpp"

namespace jadefx {

// A node that owns children.
class Parent : public Node {
protected:
    Parent() = default;
};

}  // namespace jadefx
