#include "jadefx/scene/controls/SplitPane.hpp"

#include "jadefx/paint/Color.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace jadefx {
namespace {

constexpr double kUnlimited = 1.0e15;
constexpr double kDividerPad = 4;

bool WouldCycle(const Node* node, const Node* parent) {
    if (node == nullptr || parent == nullptr) {
        return false;
    }
    for (const Node* cursor = parent; cursor != nullptr; cursor = cursor->getParent()) {
        if (cursor == node) {
            return true;
        }
    }
    return false;
}

double AxisMin(const Node* node, bool horizontal, double available) {
    if (node == nullptr) {
        return 0;
    }
    const ComputedStyle& style = node->computedStyle();
    const SizeSpec& spec = horizontal ? style.minWidth : style.minHeight;
    if (!spec.set()) {
        return 0;
    }
    return std::max(0.0, resolveSize(spec, std::max(0.0, available), style.fontSize));
}

double AxisMax(const Node* node, bool horizontal, double available) {
    if (node == nullptr) {
        return 0;
    }
    const ComputedStyle& style = node->computedStyle();
    const SizeSpec& spec = horizontal ? style.maxWidth : style.maxHeight;
    if (!spec.set()) {
        return kUnlimited;
    }
    return std::max(0.0, resolveSize(spec, std::max(0.0, available), style.fontSize));
}

// Divider thickness is the left padding plus the right padding, including border.
// OpenJFX uses that pref width for a vertical split as well.
double Thickness(const Node& node) {
    const ComputedStyle& style = node.computedStyle();
    return std::max(0.0, style.padding.left + style.padding.right + style.border.left + style.border.right);
}

class Grip : public Region {
public:
    Grip() {
        setMouseTransparent(true);
        setBackground(Color::rgb8(110, 110, 110));
        setHorizontal(true);
    }

    const char* getElementType() const override { return horizontal_ ? "horizontal-grabber" : "vertical-grabber"; }

    void setHorizontal(bool horizontal) {
        horizontal_ = horizontal;
        const char* name = horizontal ? "horizontal-grabber" : "vertical-grabber";
        ObservableList<std::string>& classes = getClassList();
        if (classes.size() != 1 || classes[0] != name) {
            classes.clear();
            classes.add(name);
        }
        if (horizontal) {
            setPrefSize(4, 32);
        } else {
            setPrefSize(32, 4);
        }
    }

private:
    bool horizontal_ = true;
};

}  // namespace

class SplitPane::ContentHost : public Region {
public:
    ContentHost(SplitPane& owner, std::shared_ptr<Node> item) : owner_(&owner), item_(std::move(item)) {
        if (item_) {
            children().add(item_);
        }
    }

    const char* getElementType() const override { return "split-content"; }

    Node* item() const { return item_.get(); }

    void releaseItem() {
        if (!item_) {
            return;
        }
        silent_ = true;
        const Node* raw = item_.get();
        children().removeIf([&](const std::shared_ptr<Node>& child) { return child.get() == raw; });
        item_.reset();
        silent_ = false;
    }

protected:
    void detachChild(Node* child) override {
        const bool ours = item_.get() == child;
        Region::detachChild(child);
        if (!ours) {
            return;
        }
        item_.reset();
        if (!silent_ && owner_ != nullptr) {
            owner_->forgetItem(child);
        }
    }

    void layoutChildren() override {
        if (!item_) {
            return;
        }
        const double innerW = std::max(0.0, getWidth());
        const double innerH = std::max(0.0, getHeight());
        const double minW = AxisMin(item_.get(), true, innerW);
        const double minH = AxisMin(item_.get(), false, innerH);
        const double maxW = AxisMax(item_.get(), true, innerW);
        const double maxH = AxisMax(item_.get(), false, innerH);
        double width = innerW;
        double height = innerH;
        if (minW > maxW) {
            width = minW;
        } else {
            width = std::min(std::max(innerW, minW), maxW);
        }
        if (minH > maxH) {
            height = minH;
        } else {
            height = std::min(std::max(innerH, minH), maxH);
        }
        const double x = (innerW - width) * 0.5;
        const double y = (innerH - height) * 0.5;
        item_->performLayout(x, y, std::max(0.0, width), std::max(0.0, height));
    }

    void renderChildren(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const bool clip = width > 0.f && height > 0.f;
        if (clip) {
            renderer.pushClip(x, y, width, height);
        }
        Region::renderChildren(renderer, opacity);
        if (clip) {
            renderer.popClip();
        }
    }

private:
    SplitPane* owner_ = nullptr;
    std::shared_ptr<Node> item_;
    bool silent_ = false;
};

class SplitPane::DividerHost : public Region {
public:
    DividerHost(SplitPane& owner, std::size_t index) : owner_(&owner), index_(index) {
        getClassList().add("split-pane-divider");
        setPadding(Insets{0, kDividerPad, 0, kDividerPad});
        setBackground(Color::rgb8(176, 176, 176));
        grip_ = std::make_shared<Grip>();
        children().add(grip_);
        sync(owner.horizontalSplit());
    }

    const char* getElementType() const override { return "split-pane-divider"; }

    void sync(bool horizontal) {
        setDefaultCursor(horizontal ? Cursor::EwResize : Cursor::NsResize);
        if (grip_) {
            grip_->setHorizontal(horizontal);
        }
    }

protected:
    void handleMousePressed(const MouseEvent& event) override {
        if (owner_ != nullptr) {
            owner_->pressDivider(index_, event);
        }
    }

    void handleMouseDragged(const MouseEvent& event) override {
        if (owner_ != nullptr) {
            owner_->dragDivider(index_, event);
        }
    }

    void layoutChildren() override {
        if (!grip_) {
            return;
        }
        const double limitW = std::max(0.0, getWidth());
        const double limitH = std::max(0.0, getHeight());
        double width = std::min(grip_->measuredWidth(limitW), limitW);
        double height = std::min(grip_->measuredHeight(width, limitH), limitH);
        const double x = (limitW - width) * 0.5;
        const double y = (limitH - height) * 0.5;
        grip_->performLayout(x, y, std::max(0.0, width), std::max(0.0, height));
    }

private:
    SplitPane* owner_ = nullptr;
    std::size_t index_ = 0;
    std::shared_ptr<Grip> grip_;
};

struct SplitPane::Impl {
    struct Band {
        std::shared_ptr<ContentHost> pane;
        double x = 0;
        double y = 0;
        double area = 0;
        double available = 0;
        double savedArea = 0;
    };

    struct Bar {
        std::shared_ptr<DividerHost> pane;
        double initialPos = 0;
        double dividerPos = -1;
        double pressPos = 0;
        double x = 0;
        double y = 0;
        bool posExplicit = false;
    };

    struct CacheEntry {
        bool keep = false;
        double position = 0.5;
    };

    ObservableList<std::shared_ptr<Node>> items;
    std::vector<Divider> dividers;
    std::vector<Band> bands;
    std::vector<Bar> bars;
    std::unordered_map<int, CacheEntry> cache;
    double previousSize = -1;
    int lastDividerUpdate = 0;
    bool resize = false;
    bool checkDividerPos = true;
    bool adjusting = false;
    bool shuttingDown = false;

    static double distributeTo(std::vector<Band*>& available, double size, bool horizontal, double limit);
    static double distributeFrom(double size, std::vector<Band*>& available);

    Node* itemAt(std::size_t index) const {
        if (index >= bands.size() || !bands[index].pane) {
            return nullptr;
        }
        return bands[index].pane->item();
    }

    bool resizableAt(std::size_t index) const {
        Node* node = itemAt(index);
        return node == nullptr || SplitPane::isResizableWithParent(*node);
    }

    double thickness() const {
        if (bars.empty() || !bars[0].pane) {
            return 0;
        }
        return Thickness(*bars[0].pane);
    }

    void layout(SplitPane& pane);
};

double SplitPane::Impl::distributeTo(std::vector<Band*>& available, double size, bool horizontal, double limit) {
    if (available.empty()) {
        return size;
    }
    size = std::ceil(size);
    int portion = static_cast<int>(size) / static_cast<int>(available.size());
    while (size > 0.0 && !available.empty()) {
        for (std::size_t index = 0; index < available.size();) {
            Band* band = available[index];
            Node* node = band->pane ? band->pane->item() : nullptr;
            const double maxSize = std::min(AxisMax(node, horizontal, limit), kUnlimited);
            const double minSize = AxisMin(node, horizontal, limit);
            if (band->area >= maxSize) {
                band->available = band->area - minSize;
                available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            if (static_cast<double>(portion) >= (maxSize - band->area)) {
                size -= (maxSize - band->area);
                band->area = maxSize;
                band->available = maxSize - minSize;
                available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
            } else {
                band->area += portion;
                band->available = band->area - minSize;
                size -= portion;
                ++index;
            }
            if (static_cast<int>(size) == 0) {
                return size;
            }
        }
        if (available.empty()) {
            return size;
        }
        portion = static_cast<int>(size) / static_cast<int>(available.size());
        const int remainder = static_cast<int>(size) % static_cast<int>(available.size());
        if (portion == 0 && remainder != 0) {
            portion = remainder;
        }
    }
    return size;
}

double SplitPane::Impl::distributeFrom(double size, std::vector<Band*>& available) {
    if (available.empty()) {
        return size;
    }
    size = std::ceil(size);
    int portion = static_cast<int>(size) / static_cast<int>(available.size());
    while (size > 0.0 && !available.empty()) {
        for (std::size_t index = 0; index < available.size();) {
            Band* band = available[index];
            if (static_cast<double>(portion) >= band->available) {
                band->area -= band->available;
                size -= band->available;
                band->available = 0;
                available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
            } else {
                band->area -= portion;
                band->available -= portion;
                size -= portion;
                ++index;
            }
            if (static_cast<int>(size) == 0) {
                return size;
            }
        }
        if (available.empty()) {
            return size;
        }
        portion = static_cast<int>(size) / static_cast<int>(available.size());
        const int remainder = static_cast<int>(size) % static_cast<int>(available.size());
        if (portion == 0 && remainder != 0) {
            portion = remainder;
        }
    }
    return size;
}

void SplitPane::Impl::layout(SplitPane& pane) {
    const bool horizontal = pane.horizontalSplit();
    const double span = horizontal ? pane.contentWidth() : pane.contentHeight();
    const double cross = horizontal ? pane.contentHeight() : pane.contentWidth();
    if (span <= 0.0 || bands.empty()) {
        return;
    }

    const double barSize = thickness();
    auto itemOf = [&](const Band& band) -> Node* { return band.pane ? band.pane->item() : nullptr; };
    auto totalMin = [&]() {
        double minSize = barSize * static_cast<double>(bars.size());
        for (const Band& band : bands) {
            minSize += AxisMin(itemOf(band), horizontal, span);
        }
        return minSize;
    };
    auto getSize = [&]() {
        const double minSize = totalMin();
        return span > minSize ? span : minSize;
    };
    auto posToEdge = [&](double pos) {
        double edge = getSize() * pos;
        if (pos == 1.0) {
            edge -= barSize;
        } else {
            edge -= barSize * 0.5;
        }
        return std::round(edge);
    };
    auto setAbsolute = [&](std::size_t index, double value) {
        if (pane.getWidth() <= 0.0 || pane.getHeight() <= 0.0 || index >= bars.size() || index >= dividers.size()) {
            return;
        }
        bars[index].dividerPos = value;
        const double size = getSize();
        if (size != 0.0) {
            dividers[index].setPosition((value + barSize * 0.5) / size);
        } else {
            dividers[index].setPosition(0);
        }
    };
    auto edgeOf = [&](std::size_t index) {
        if (pane.getWidth() <= 0.0 || pane.getHeight() <= 0.0 || index >= bars.size() || index >= dividers.size()) {
            return 0.0;
        }
        const double edge = posToEdge(dividers[index].getPosition());
        bars[index].dividerPos = edge;
        return edge;
    };
    auto checkEdge = [&](std::size_t index, double newPos, double oldPos) {
        if (index >= bars.size()) {
            return;
        }
        Node* left = itemOf(bands[index]);
        Node* right = index + 1 < bands.size() ? itemOf(bands[index + 1]) : nullptr;
        const double minLeft = AxisMin(left, horizontal, span);
        const double minRight = AxisMin(right, horizontal, span);
        const double maxLeft = AxisMax(left, horizontal, span);
        const double maxRight = AxisMax(right, horizontal, span);
        double previousEdge = 0;
        double nextEdge = getSize();
        if (index > 0) {
            previousEdge = bars[index - 1].dividerPos;
            if (previousEdge < 0.0) {
                previousEdge = edgeOf(index - 1);
            }
        }
        if (index + 1 < bars.size()) {
            nextEdge = bars[index + 1].dividerPos;
            if (nextEdge < 0.0) {
                nextEdge = edgeOf(index + 1);
            }
        }
        const bool saved = checkDividerPos;
        checkDividerPos = false;
        if (newPos > oldPos) {
            const double maxEdge = previousEdge == 0.0 ? maxLeft : previousEdge + barSize + maxLeft;
            const double minEdge = nextEdge - minRight - barSize;
            const double stop = std::min(maxEdge, minEdge);
            if (newPos >= stop) {
                setAbsolute(index, stop);
            } else {
                const double rightMax = nextEdge - maxRight - barSize;
                setAbsolute(index, newPos <= rightMax ? rightMax : newPos);
            }
        } else {
            const double maxEdge = nextEdge - maxRight - barSize;
            const double minEdge = previousEdge == 0.0 ? minLeft : previousEdge + minLeft + barSize;
            const double stop = std::max(maxEdge, minEdge);
            if (newPos <= stop) {
                setAbsolute(index, stop);
            } else {
                const double leftMax = previousEdge + maxLeft + barSize;
                setAbsolute(index, newPos >= leftMax ? leftMax : newPos);
            }
        }
        checkDividerPos = saved;
    };
    auto setAndCheck = [&](std::size_t index, double value) {
        const double oldPos = bars[index].dividerPos;
        setAbsolute(index, value);
        checkEdge(index, value, oldPos);
    };
    auto setup = [&]() {
        double cursorX = 0;
        double cursorY = 0;
        for (Band& band : bands) {
            if (resize && !resizableAt(static_cast<std::size_t>(&band - bands.data()))) {
                band.area = band.savedArea;
            }
            band.x = cursorX;
            band.y = cursorY;
            if (horizontal) {
                cursorX += band.area + barSize;
            } else {
                cursorY += band.area + barSize;
            }
        }
        cursorX = 0;
        cursorY = 0;
        const bool saved = checkDividerPos;
        checkDividerPos = false;
        for (std::size_t i = 0; i < bars.size() && i < bands.size(); ++i) {
            if (horizontal) {
                cursorX += bands[i].area + (i == 0 ? 0 : barSize);
            } else {
                cursorY += bands[i].area + (i == 0 ? 0 : barSize);
            }
            bars[i].x = cursorX;
            bars[i].y = cursorY;
            setAbsolute(i, horizontal ? bars[i].x : bars[i].y);
            bars[i].posExplicit = false;
        }
        checkDividerPos = saved;
    };
    auto place = [&]() {
        const double originX = pane.contentLeft();
        const double originY = pane.contentTop();
        for (Band& band : bands) {
            if (!band.pane) {
                continue;
            }
            const double width = horizontal ? band.area : cross;
            const double height = horizontal ? cross : band.area;
            band.pane->performLayout(originX + band.x, originY + band.y, std::max(0.0, width), std::max(0.0, height));
        }
        for (Bar& bar : bars) {
            if (!bar.pane) {
                continue;
            }
            const double width = horizontal ? barSize : cross;
            const double height = horizontal ? cross : barSize;
            bar.pane->performLayout(originX + bar.x, originY + bar.y, std::max(0.0, width), std::max(0.0, height));
        }
    };

    resize = false;
    if (!bars.empty() && previousSize >= 0.0 && previousSize != span) {
        std::vector<Band*> resizeList;
        for (std::size_t i = 0; i < bands.size(); ++i) {
            if (resizableAt(i)) {
                resizeList.push_back(&bands[i]);
            }
        }
        double delta = span - previousSize;
        const bool growing = delta > 0.0;
        delta = std::fabs(delta);
        if (delta != 0.0 && !resizeList.empty()) {
            int portion = static_cast<int>(delta) / static_cast<int>(resizeList.size());
            int remainder = static_cast<int>(delta) % static_cast<int>(resizeList.size());
            int left = 0;
            if (portion == 0) {
                portion = remainder;
                left = remainder;
                remainder = 0;
            } else {
                left = portion * static_cast<int>(resizeList.size());
            }
            while (left > 0 && !resizeList.empty()) {
                if (growing) {
                    ++lastDividerUpdate;
                } else {
                    --lastDividerUpdate;
                    if (lastDividerUpdate < 0) {
                        lastDividerUpdate = static_cast<int>(bands.size()) - 1;
                    }
                }
                if (bands.empty()) {
                    break;
                }
                const int id = lastDividerUpdate % static_cast<int>(bands.size());
                if (id < 0) {
                    break;
                }
                Band& band = bands[static_cast<std::size_t>(id)];
                const auto listed = std::find(resizeList.begin(), resizeList.end(), &band);
                if (resizableAt(static_cast<std::size_t>(id)) && listed != resizeList.end()) {
                    double area = band.area;
                    if (growing) {
                        const double maxSize = AxisMax(itemOf(band), horizontal, span);
                        if (area + portion <= maxSize) {
                            area += portion;
                        } else {
                            resizeList.erase(listed);
                            continue;
                        }
                    } else {
                        const double minSize = AxisMin(itemOf(band), horizontal, span);
                        if (area - portion >= minSize) {
                            area -= portion;
                        } else {
                            resizeList.erase(listed);
                            continue;
                        }
                    }
                    band.area = area;
                    left -= portion;
                    if (left == 0 && remainder != 0) {
                        portion = remainder;
                        left = remainder;
                        remainder = 0;
                    } else if (left == 0) {
                        break;
                    }
                }
            }
            for (std::size_t i = 0; i < bands.size(); ++i) {
                bands[i].savedArea = resizableAt(i) ? 0 : bands[i].area;
                bands[i].available = 0;
            }
            resize = true;
        }
    }
    previousSize = span;

    const double minSize = totalMin();
    if (minSize > span) {
        for (Band& band : bands) {
            const double minimum = AxisMin(itemOf(band), horizontal, span);
            const double share = minSize <= 0.0 ? 0.0 : minimum / minSize;
            band.area = std::round(share * span);
            band.available = 0;
        }
        setup();
        place();
        resize = false;
        return;
    }

    for (int attempt = 0; attempt < 10; ++attempt) {
        int current = -1;
        bool havePrevious = false;
        std::size_t previousIndex = 0;
        for (std::size_t i = 0; i < bands.size(); ++i) {
            double space = 0;
            if (i < bars.size()) {
                current = static_cast<int>(i);
                if (bars[i].posExplicit) {
                    checkEdge(i, posToEdge(dividers[i].getPosition()), bars[i].dividerPos);
                }
                if (i == 0) {
                    space = edgeOf(i);
                } else {
                    const double previousEdge = edgeOf(i - 1);
                    const double parked = previousEdge + barSize;
                    if (edgeOf(i) <= previousEdge) {
                        setAndCheck(i, parked);
                    }
                    space = edgeOf(i) - parked;
                }
                havePrevious = true;
                previousIndex = i;
            } else if (i == bars.size()) {
                space = span - (havePrevious ? edgeOf(previousIndex) + barSize : 0);
            }
            const bool explicitPos = current >= 0 && bars[static_cast<std::size_t>(current)].posExplicit;
            if (!resize || explicitPos || current < 0) {
                bands[i].area = space;
            }
        }

        double spaceRequested = 0;
        double extraSpace = 0;
        for (Band& band : bands) {
            Node* node = itemOf(band);
            const double maxSize = AxisMax(node, horizontal, span);
            const double minLimit = AxisMin(node, horizontal, span);
            if (band.area >= maxSize) {
                extraSpace += band.area - maxSize;
                band.area = maxSize;
            }
            band.available = band.area - minLimit;
            if (band.available < 0.0) {
                spaceRequested += band.available;
            }
        }
        spaceRequested = std::fabs(spaceRequested);

        std::vector<Band*> availableList;
        std::vector<Band*> storageList;
        std::vector<Band*> requestors;
        double available = 0;
        for (std::size_t i = 0; i < bands.size(); ++i) {
            Band& band = bands[i];
            if (band.available >= 0.0) {
                available += band.available;
                availableList.push_back(&band);
            }
            if (resize && !resizableAt(i)) {
                if (band.area >= band.savedArea) {
                    extraSpace += band.area - band.savedArea;
                } else {
                    spaceRequested += band.savedArea - band.area;
                }
                band.available = 0;
            }
            if (!resize || resizableAt(i)) {
                storageList.push_back(&band);
            }
            if (band.available < 0.0) {
                requestors.push_back(&band);
            }
        }

        if (extraSpace > 0.0) {
            distributeTo(storageList, extraSpace, horizontal, span);
            spaceRequested = 0;
            requestors.clear();
            available = 0;
            availableList.clear();
            for (Band& band : bands) {
                if (band.available < 0.0) {
                    spaceRequested += band.available;
                    requestors.push_back(&band);
                } else {
                    available += band.available;
                    availableList.push_back(&band);
                }
            }
            spaceRequested = std::fabs(spaceRequested);
        }

        if (available >= spaceRequested) {
            for (Band* requestor : requestors) {
                Node* node = itemOf(*requestor);
                requestor->area = AxisMin(node, horizontal, span);
                requestor->available = 0;
            }
            if (spaceRequested > 0.0 && !requestors.empty()) {
                distributeFrom(spaceRequested, availableList);
            }
            if (resize) {
                double total = barSize * static_cast<double>(bars.size());
                for (std::size_t i = 0; i < bands.size(); ++i) {
                    total += resizableAt(i) ? bands[i].area : bands[i].savedArea;
                }
                if (total < span) {
                    distributeTo(storageList, span - total, horizontal, span);
                } else {
                    distributeFrom(total - span, storageList);
                }
            }
        }

        setup();

        bool passed = true;
        for (const Band& band : bands) {
            Node* node = itemOf(band);
            const double maxSize = AxisMax(node, horizontal, span);
            const double minLimit = AxisMin(node, horizontal, span);
            if (band.area < minLimit || band.area > maxSize) {
                passed = false;
                break;
            }
        }
        if (passed) {
            break;
        }
    }

    place();
    resize = false;
}

void SplitPane::Divider::setPosition(double value) {
    if (position_ == value) {
        return;
    }
    position_ = value;
    if (owner_ != nullptr) {
        owner_->onDividerPositionChanged(this);
    }
}

SplitPane::SplitPane() : impl_(std::make_unique<Impl>()) {
    getClassList().add("split-pane");
    setBackground(Color::rgb8(244, 244, 244));
    setPseudoState("horizontal", true);
    impl_->items.setIndexedAddCallback([this](std::shared_ptr<Node> item, std::size_t index) {
        onAdded(std::move(item), index);
    });
    impl_->items.setIndexedRemoveCallback([this](std::shared_ptr<Node> item, std::size_t index) {
        onRemoved(std::move(item), index);
    });
}

SplitPane::~SplitPane() {
    if (!impl_) {
        return;
    }
    impl_->shuttingDown = true;
    impl_->items.setIndexedAddCallback(nullptr);
    impl_->items.setIndexedRemoveCallback(nullptr);
    for (Impl::Band& band : impl_->bands) {
        if (band.pane) {
            band.pane->releaseItem();
        }
    }
    impl_->bands.clear();
    impl_->bars.clear();
}

ObservableList<std::shared_ptr<Node>>& SplitPane::getItems() { return impl_->items; }

const ObservableList<std::shared_ptr<Node>>& SplitPane::getItems() const { return impl_->items; }

std::vector<SplitPane::Divider>& SplitPane::getDividers() { return impl_->dividers; }

const std::vector<SplitPane::Divider>& SplitPane::getDividers() const { return impl_->dividers; }

void SplitPane::setDividerPosition(int dividerIndex, double position) {
    if (dividerIndex < 0 || !impl_) {
        return;
    }
    if (static_cast<int>(impl_->dividers.size()) <= dividerIndex) {
        impl_->cache[dividerIndex] = Impl::CacheEntry{true, position};
        return;
    }
    impl_->dividers[static_cast<std::size_t>(dividerIndex)].setPosition(position);
}

void SplitPane::setDividerPositions(std::initializer_list<double> positions) {
    if (!impl_) {
        return;
    }
    if (impl_->dividers.empty()) {
        int index = 0;
        for (double position : positions) {
            impl_->cache[index] = Impl::CacheEntry{true, position};
            ++index;
        }
        return;
    }
    std::size_t index = 0;
    for (double position : positions) {
        if (index >= impl_->dividers.size()) {
            break;
        }
        impl_->dividers[index].setPosition(position);
        ++index;
    }
}

std::vector<double> SplitPane::getDividerPositions() const {
    std::vector<double> positions;
    if (!impl_) {
        return positions;
    }
    positions.reserve(impl_->dividers.size());
    for (const Divider& divider : impl_->dividers) {
        positions.push_back(divider.getPosition());
    }
    return positions;
}

void SplitPane::setOrientation(Orientation orientation) {
    const bool changed = orientation_ != orientation;
    orientation_ = orientation;
    if (changed && impl_) {
        impl_->previousSize = -1;
    }
    syncChrome();
}

void SplitPane::setResizableWithParent(Node& node, std::optional<bool> value) { node.resizableWithParent_ = value; }

bool SplitPane::isResizableWithParent(const Node& node) {
    return node.resizableWithParent_.value_or(true);
}

void SplitPane::layoutChildren() {
    if (impl_) {
        impl_->layout(*this);
    }
}

void SplitPane::styleDidApply() {
    if (computed().orientationFromCss) {
        setOrientation(computed().orientation);
        return;
    }
    syncChrome();
}

double SplitPane::preferredContentWidth(double innerAvailable) const {
    if (!impl_) {
        return 0;
    }
    const double limit = innerAvailable > 0 ? innerAvailable : 0;
    double along = 0;
    double cross = 0;
    for (const Impl::Band& band : impl_->bands) {
        Node* item = band.pane ? band.pane->item() : nullptr;
        if (item == nullptr) {
            continue;
        }
        const double width = item->measuredWidth(limit);
        along += width;
        cross = std::max(cross, width);
    }
    if (horizontalSplit()) {
        return along + impl_->thickness() * static_cast<double>(impl_->bars.size());
    }
    return cross;
}

double SplitPane::preferredContentHeight(double innerWidth) const {
    if (!impl_) {
        return 0;
    }
    double along = 0;
    double cross = 0;
    for (const Impl::Band& band : impl_->bands) {
        Node* item = band.pane ? band.pane->item() : nullptr;
        if (item == nullptr) {
            continue;
        }
        const double height = item->measuredHeight(innerWidth, -1);
        along += height;
        cross = std::max(cross, height);
    }
    if (horizontalSplit()) {
        return cross;
    }
    return along + impl_->thickness() * static_cast<double>(impl_->bars.size());
}

void SplitPane::onAdded(std::shared_ptr<Node> item, std::size_t index) {
    if (!impl_ || impl_->adjusting || impl_->shuttingDown) {
        return;
    }
    bool duplicate = false;
    for (std::size_t i = 0; i < impl_->items.size(); ++i) {
        if (i != index && impl_->items[i].get() == item.get()) {
            duplicate = true;
        }
    }
    if (!item || item.get() == this || WouldCycle(item.get(), this) || duplicate) {
        impl_->adjusting = true;
        impl_->items.removeAt(index);
        impl_->adjusting = false;
        return;
    }
    insertBand(index, item);
    relayoutFromItems(static_cast<int>(index), 0);
}

void SplitPane::onRemoved(std::shared_ptr<Node> item, std::size_t index) {
    (void)item;
    if (!impl_ || impl_->adjusting || impl_->shuttingDown) {
        return;
    }
    impl_->adjusting = true;
    removeBand(index);
    relayoutFromItems(static_cast<int>(index), 1);
    impl_->adjusting = false;
}

void SplitPane::forgetItem(Node* item) {
    if (!impl_ || impl_->adjusting || impl_->shuttingDown || item == nullptr) {
        return;
    }
    impl_->items.removeIf([&](const std::shared_ptr<Node>& node) { return node.get() == item; });
}

void SplitPane::insertBand(std::size_t index, const std::shared_ptr<Node>& item) {
    auto pane = std::shared_ptr<ContentHost>(new ContentHost(*this, item));
    if (index > impl_->bands.size()) {
        index = impl_->bands.size();
    }
    impl_->bands.insert(impl_->bands.begin() + static_cast<std::ptrdiff_t>(index), Impl::Band{pane});
    children().insert(index, pane);
}

void SplitPane::removeBand(std::size_t index) {
    if (index >= impl_->bands.size()) {
        return;
    }
    std::shared_ptr<ContentHost> pane = impl_->bands[index].pane;
    impl_->bands.erase(impl_->bands.begin() + static_cast<std::ptrdiff_t>(index));
    if (!pane) {
        return;
    }
    pane->releaseItem();
    children().removeIf([&](const std::shared_ptr<Node>& child) { return child.get() == pane.get(); });
}

void SplitPane::relayoutFromItems(int from, int removedCount) {
    if (!impl_) {
        return;
    }
    const int oldCount = static_cast<int>(impl_->dividers.size());
    int index = from;
    for (int i = 0; i < removedCount; ++i) {
        if (index < oldCount) {
            impl_->cache[index] = Impl::CacheEntry{false, 0};
        } else if (index == oldCount && oldCount > 0) {
            impl_->cache[index - 1] = Impl::CacheEntry{false, 0};
        }
        ++index;
    }
    for (int i = 0; i < oldCount; ++i) {
        if (impl_->cache.find(i) == impl_->cache.end()) {
            impl_->cache[i] = Impl::CacheEntry{true, impl_->dividers[static_cast<std::size_t>(i)].getPosition()};
        }
    }
    rebuildDividers();
}

void SplitPane::rebuildDividers() {
    for (Impl::Bar& bar : impl_->bars) {
        if (!bar.pane) {
            continue;
        }
        DividerHost* raw = bar.pane.get();
        children().removeIf([&](const std::shared_ptr<Node>& child) { return child.get() == raw; });
    }
    impl_->bars.clear();
    impl_->dividers.clear();
    impl_->lastDividerUpdate = 0;

    const int count = static_cast<int>(impl_->items.size()) - 1;
    if (count <= 0) {
        return;
    }
    impl_->dividers.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        Divider divider;
        const auto found = impl_->cache.find(i);
        if (found != impl_->cache.end() && found->second.keep) {
            divider.position_ = found->second.position;
        }
        divider.owner_ = this;
        impl_->dividers.push_back(divider);
        impl_->cache.erase(i);
    }
    for (int i = 0; i < count; ++i) {
        auto pane = std::shared_ptr<DividerHost>(new DividerHost(*this, static_cast<std::size_t>(i)));
        Impl::Bar bar;
        bar.pane = pane;
        impl_->bars.push_back(std::move(bar));
        children().add(std::move(pane));
    }
    syncChrome();
}

void SplitPane::syncChrome() {
    const bool horizontal = horizontalSplit();
    setPseudoState("horizontal", horizontal);
    setPseudoState("vertical", !horizontal);
    if (!impl_) {
        return;
    }
    for (Impl::Bar& bar : impl_->bars) {
        if (bar.pane) {
            bar.pane->sync(horizontal);
        }
    }
}

void SplitPane::pressDivider(std::size_t index, const MouseEvent& event) {
    if (!impl_ || index >= impl_->bars.size()) {
        return;
    }
    Impl::Bar& bar = impl_->bars[index];
    bar.initialPos = bar.dividerPos >= 0.0 ? bar.dividerPos : (horizontalSplit() ? bar.x : bar.y);
    bar.pressPos = horizontalSplit() ? event.x : event.y;
}

void SplitPane::dragDivider(std::size_t index, const MouseEvent& event) {
    if (!impl_ || index >= impl_->bars.size()) {
        return;
    }
    // The pointer delta is in window points. initialPos is the divider's edge inside the pane.
    const double current = horizontalSplit() ? event.x : event.y;
    const double edge = std::ceil(impl_->bars[index].initialPos + (current - impl_->bars[index].pressPos));
    impl_->checkDividerPos = true;
    const double oldPos = impl_->bars[index].dividerPos;
    const double barSize = impl_->thickness();
    auto setEdge = [&](double value) {
        if (getWidth() <= 0.0 || getHeight() <= 0.0 || index >= impl_->dividers.size()) {
            return;
        }
        impl_->bars[index].dividerPos = value;
        const double span = contentSpan();
        const double minSize = [&]() {
            double total = barSize * static_cast<double>(impl_->bars.size());
            const bool horizontal = horizontalSplit();
            for (const Impl::Band& band : impl_->bands) {
                total += AxisMin(band.pane ? band.pane->item() : nullptr, horizontal, span);
            }
            return total;
        }();
        const double size = span > minSize ? span : minSize;
        if (size != 0.0) {
            impl_->dividers[index].setPosition((value + barSize * 0.5) / size);
        }
    };
    setEdge(edge);
    // Clamp against the neighbor minimum and maximum sizes. Layout repeats this.
    if (index < impl_->bands.size()) {
        const bool horizontal = horizontalSplit();
        Node* left = impl_->bands[index].pane ? impl_->bands[index].pane->item() : nullptr;
        Node* right = index + 1 < impl_->bands.size() && impl_->bands[index + 1].pane ? impl_->bands[index + 1].pane->item()
                                                                                      : nullptr;
        const double span = contentSpan();
        double previousEdge = 0;
        double nextEdge = span;
        if (index > 0) {
            previousEdge = impl_->bars[index - 1].dividerPos;
        }
        if (index + 1 < impl_->bars.size() && impl_->bars[index + 1].dividerPos >= 0.0) {
            nextEdge = impl_->bars[index + 1].dividerPos;
        }
        impl_->checkDividerPos = false;
        if (edge > oldPos) {
            const double maxEdge = previousEdge == 0.0 ? AxisMax(left, horizontal, span) : previousEdge + barSize + AxisMax(left, horizontal, span);
            const double minEdge = nextEdge - AxisMin(right, horizontal, span) - barSize;
            const double stop = std::min(maxEdge, minEdge);
            if (edge >= stop) {
                setEdge(stop);
            } else {
                const double rightMax = nextEdge - AxisMax(right, horizontal, span) - barSize;
                setEdge(edge <= rightMax ? rightMax : edge);
            }
        } else {
            const double maxEdge = nextEdge - AxisMax(right, horizontal, span) - barSize;
            const double minEdge = previousEdge == 0.0 ? AxisMin(left, horizontal, span) : previousEdge + AxisMin(left, horizontal, span) + barSize;
            const double stop = std::max(maxEdge, minEdge);
            if (edge <= stop) {
                setEdge(stop);
            } else {
                const double leftMax = previousEdge + AxisMax(left, horizontal, span) + barSize;
                setEdge(edge >= leftMax ? leftMax : edge);
            }
        }
        impl_->checkDividerPos = true;
    }
    if (index < impl_->bars.size()) {
        impl_->bars[index].posExplicit = true;
    }
}

void SplitPane::onDividerPositionChanged(Divider* divider) {
    if (!impl_ || !impl_->checkDividerPos || divider == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < impl_->dividers.size() && i < impl_->bars.size(); ++i) {
        if (&impl_->dividers[i] == divider) {
            impl_->bars[i].posExplicit = true;
        }
    }
}

double SplitPane::contentSpan() const { return horizontalSplit() ? contentWidth() : contentHeight(); }

}  // namespace jadefx
