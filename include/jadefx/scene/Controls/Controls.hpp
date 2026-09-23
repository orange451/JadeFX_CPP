#pragma once

#include "jadefx/scene/layout/Region.hpp"

namespace jadefx {

// Base type for every control, so a node can be tested as one.
class Controls : public Region {
protected:
    Controls() = default;
};

}  // namespace jadefx
