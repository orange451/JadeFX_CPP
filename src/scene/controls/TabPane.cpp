#include "jadefx/scene/controls/TabPane.hpp"

#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "gl/UiRenderer.hpp"
#include "../layout/LayoutDetail.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace jadefx {
namespace {

constexpr double kTabGap = 2;
constexpr double kInnerGap = 6;
constexpr double kUnlimited = 1.0e9;

struct InputGuard {
    int& depth;
    explicit InputGuard(int& depth) : depth(depth) { ++depth; }
    ~InputGuard() { --depth; }
    InputGuard(const InputGuard&) = delete;
    InputGuard& operator=(const InputGuard&) = delete;
};

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

double ClampAxis(double value, double minimum, double maximum) {
    const double floor = std::max(0.0, minimum);
    const double ceiling = std::max(floor, maximum);
    if (value < floor) {
        return floor;
    }
    if (value > ceiling) {
        return ceiling;
    }
    return value;
}

double Center(double space, double child) {
    const double extra = space - child;
    return extra > 0 ? extra * 0.5 : 0;
}

// Shrink sizes toward their floors until they fit in limit.
void FitDown(std::vector<double>& sizes, const std::vector<double>& floors, double limit) {
    if (sizes.empty()) {
        return;
    }
    double sum = 0;
    for (double size : sizes) {
        sum += size;
    }
    for (int pass = 0; pass < 8 && sum > limit; ++pass) {
        double room = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            const double floor = i < floors.size() ? floors[i] : 0;
            room += std::max(0.0, sizes[i] - floor);
        }
        if (room <= 0.01) {
            break;
        }
        const double take = std::min(sum - limit, room);
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            const double floor = i < floors.size() ? floors[i] : 0;
            const double slack = std::max(0.0, sizes[i] - floor);
            if (slack <= 0) {
                continue;
            }
            sizes[i] -= take * (slack / room);
            if (sizes[i] < floor) {
                sizes[i] = floor;
            }
        }
        sum = 0;
        for (double size : sizes) {
            sum += size;
        }
    }
}

class TabLabel : public Label {
public:
    explicit TabLabel(std::string text) : Label(std::move(text)) {
        getClassList().add("tab-label");
        setAlignment(Pos::Center);
    }

    const char* getElementType() const override { return "tab-label"; }
};

class TabCloseButton : public Label {
public:
    TabCloseButton() : Label("\u00d7") {
        setDefaultCursor(Cursor::Pointer);
        getClassList().add("tab-close-button");
        setAlignment(Pos::Center);
        setMinSize(16, 16);
        setFont(Font("Open Sans", 14.f));
    }

    const char* getElementType() const override { return "tab-close-button"; }
};

struct HeaderState {
    std::string text;
    std::string id;
    std::string style;
    std::vector<std::string> classes;
    std::shared_ptr<Node> graphic;
    bool selected = false;
    bool disabled = false;
    bool showClose = false;
};

bool HorizontalSide(Side side) { return side == Side::Top || side == Side::Bottom; }

}  // namespace

class TabPane::HeaderBar : public Region {
public:
    HeaderBar() {
        getClassList().add("tab-header-area");
        setMouseTransparent(true);
        setBackground(Color::rgb8(241, 243, 244));
    }

    const char* getElementType() const override { return "tab-header-area"; }

    void setEdge(Side side) { edge_ = side; }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 0.f || height <= 0.f) {
            return;
        }
        Color color = Color::rgb8(218, 220, 224);
        color.a *= opacity;
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float radius[4] = {};
        const float at = 0.f;
        const float thickness = 1.f;
        float lineX = x;
        float lineY = y;
        float lineW = width;
        float lineH = thickness;
        if (edge_ == Side::Top) {
            lineY = y + height - thickness;
        } else if (edge_ == Side::Bottom) {
            lineY = y;
        } else if (edge_ == Side::Left) {
            lineX = x + width - thickness;
            lineW = thickness;
            lineH = height;
        } else {
            lineW = thickness;
            lineH = height;
        }
        renderer.fillRounded(lineX, lineY, lineW, lineH, radius, &color, &at, 1, 0.f);
    }

private:
    Side edge_ = Side::Top;
};

class TabPane::TabHeader : public Region {
public:
    TabHeader(TabPane& pane, std::shared_ptr<Tab> tab);
    ~TabHeader() override;

    const char* getElementType() const override { return "tab"; }

    void apply(HeaderState state);
    void detachChild(Node* child) override;

protected:
    void layoutChildren() override;
    void visitChildren(const std::function<void(Node*)>& visitor) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    void clickedClose();
    void clickedHeader();
    void adoptGraphic(std::shared_ptr<Node> graphic);
    void replaceLabel();
    void replaceClose();

    TabPane* pane_ = nullptr;
    std::shared_ptr<Tab> tab_;
    std::shared_ptr<Node> graphic_;
    std::shared_ptr<TabLabel> label_;
    std::shared_ptr<TabCloseButton> close_;
    bool notifyDetach_ = true;
};

struct TabPane::Impl {
    struct Slot {
        std::shared_ptr<Tab> tab;
        std::shared_ptr<TabHeader> header;
    };

    ObservableList<std::shared_ptr<Tab>> tabs;
    std::vector<Slot> slots;
    std::vector<std::shared_ptr<TabHeader>> graveyard;
    std::shared_ptr<HeaderBar> bar;
    std::shared_ptr<Tab> selected;
    std::shared_ptr<Node> shown;
    std::shared_ptr<Tab> pendingClose;
    Side side = Side::Top;
    TabClosingPolicy policy = TabClosingPolicy::SelectedTab;
    double tabMinWidth = 0;
    double tabMinHeight = 0;
    double tabMaxWidth = kUnlimited;
    double tabMaxHeight = kUnlimited;
    bool disabled = false;
    bool suppressHeader = false;
    bool suppressRemove = false;
    bool closing = false;
    bool syncing = false;
    int eventDepth = 0;
};

TabPane::TabHeader::TabHeader(TabPane& pane, std::shared_ptr<Tab> tab) : pane_(&pane), tab_(std::move(tab)) {
    setDefaultCursor(Cursor::Pointer);
    getClassList().add("tab");
    setMinSize(0, 28);
    setPadding(Insets{6, 12, 6, 12});
    replaceLabel();
    replaceClose();
    setOnMouseClicked([this](const MouseEvent&) { clickedHeader(); });
}

TabPane::TabHeader::~TabHeader() {
    notifyDetach_ = false;
    auto release = [this](auto& node) {
        if (node && node->getParent() == this) {
            node->setParent(nullptr);
        }
        node.reset();
    };
    release(graphic_);
    release(label_);
    release(close_);
}

void TabPane::TabHeader::replaceLabel() {
    label_ = std::make_shared<TabLabel>(tab_ ? tab_->getText() : std::string());
    label_->setParent(this);
}

void TabPane::TabHeader::replaceClose() {
    close_ = std::make_shared<TabCloseButton>();
    close_->setParent(this);
    close_->setOnMouseClicked([this](const MouseEvent&) { clickedClose(); });
}

void TabPane::TabHeader::clickedClose() {
    if (pane_ == nullptr || !tab_) {
        return;
    }
    // Keep this header alive through the click. Removal parks it until a later layout.
    InputGuard guard(pane_->impl_->eventDepth);
    pane_->impl_->suppressHeader = true;
    pane_->requestClose(tab_);
}

void TabPane::TabHeader::clickedHeader() {
    if (pane_ == nullptr || !tab_) {
        return;
    }
    InputGuard guard(pane_->impl_->eventDepth);
    if (pane_->impl_->suppressHeader) {
        pane_->impl_->suppressHeader = false;
        return;
    }
    if (tab_->isDisabled() || tab_->getTabPane() != pane_) {
        return;
    }
    pane_->select(tab_);
}

void TabPane::TabHeader::adoptGraphic(std::shared_ptr<Node> graphic) {
    if (graphic_ == graphic) {
        if (graphic_ && graphic_->getParent() != this && !WouldCycle(graphic_.get(), this)) {
            if (graphic_->getParent() != nullptr) {
                graphic_->getParent()->detachChild(graphic_.get());
            }
            if (graphic_) {
                graphic_->setParent(this);
            }
        }
        return;
    }
    const bool notify = notifyDetach_;
    notifyDetach_ = false;
    std::shared_ptr<Node> previous = std::move(graphic_);
    if (previous && previous->getParent() == this) {
        previous->setParent(nullptr);
    }
    notifyDetach_ = notify;
    graphic_.reset();
    if (!graphic || WouldCycle(graphic.get(), this)) {
        return;
    }
    graphic_ = std::move(graphic);
    if (graphic_->getParent() != nullptr && graphic_->getParent() != this) {
        graphic_->getParent()->detachChild(graphic_.get());
    }
    if (graphic_ && graphic_->getParent() != this) {
        graphic_->setParent(this);
    }
}

void TabPane::TabHeader::apply(HeaderState state) {
    if (getElementId() != state.id) {
        setElementId(std::move(state.id));
    }
    if (getStyle() != state.style) {
        setStyle(std::move(state.style));
    }
    const std::vector<std::string>& have = getClassList().items();
    if (have != state.classes) {
        getClassList().clear();
        for (const std::string& name : state.classes) {
            getClassList().add(name);
        }
    }
    if (label_) {
        label_->setText(std::move(state.text));
    }
    adoptGraphic(std::move(state.graphic));
    if (close_) {
        close_->setVisible(state.showClose);
    }
    setSelected(state.selected);
    setOpacity(state.disabled ? 0.45f : 1.f);
    setBackground(state.selected ? Color::white() : Color::rgb8(218, 220, 224));
}

void TabPane::TabHeader::detachChild(Node* child) {
    if (child == nullptr) {
        return;
    }
    if (graphic_.get() == child) {
        const bool notify = notifyDetach_;
        notifyDetach_ = false;
        std::shared_ptr<Node> lost = std::move(graphic_);
        if (lost && lost->getParent() == this) {
            lost->setParent(nullptr);
        }
        notifyDetach_ = notify;
        if (notify && pane_ != nullptr) {
            pane_->releaseGraphic(child);
        }
        return;
    }
    if (label_.get() == child) {
        if (label_->getParent() == this) {
            label_->setParent(nullptr);
        }
        label_.reset();
        replaceLabel();
        return;
    }
    if (close_.get() == child) {
        if (close_->getParent() == this) {
            close_->setParent(nullptr);
        }
        close_.reset();
        replaceClose();
        return;
    }
}

void TabPane::TabHeader::visitChildren(const std::function<void(Node*)>& visitor) {
    if (graphic_) {
        visitor(graphic_.get());
    }
    if (label_) {
        visitor(label_.get());
    }
    if (close_) {
        visitor(close_.get());
    }
}

double TabPane::TabHeader::preferredContentWidth(double innerAvailable) const {
    double width = 0;
    bool placed = false;
    auto append = [&](double item) {
        if (item < 0) {
            item = 0;
        }
        if (placed) {
            width += kInnerGap;
        }
        width += item;
        placed = true;
    };
    if (graphic_ && graphic_->isVisible()) {
        append(graphic_->measuredWidth(innerAvailable));
    }
    if (label_) {
        append(label_->measuredWidth(innerAvailable));
    }
    if (close_ && close_->isVisible()) {
        append(close_->measuredWidth(innerAvailable));
    }
    return width;
}

double TabPane::TabHeader::preferredContentHeight(double innerWidth) const {
    double height = 0;
    if (graphic_ && graphic_->isVisible()) {
        const double width = graphic_->measuredWidth(innerWidth);
        height = std::max(height, graphic_->measuredHeight(width, -1));
    }
    if (label_) {
        const double width = label_->measuredWidth(innerWidth);
        height = std::max(height, label_->measuredHeight(width, -1));
    }
    if (close_ && close_->isVisible()) {
        const double width = close_->measuredWidth(innerWidth);
        height = std::max(height, close_->measuredHeight(width, -1));
    }
    return height;
}

void TabPane::TabHeader::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const double boxW = contentWidth();
    const double boxH = contentHeight();
    const double right = left + boxW;
    double x = left;
    if (graphic_ && graphic_->isVisible()) {
        const double graphicW = std::min(boxW, std::max(0.0, graphic_->measuredWidth(boxW)));
        const double graphicH = std::min(boxH, std::max(0.0, graphic_->measuredHeight(graphicW, boxH)));
        graphic_->performLayout(x, top + Center(boxH, graphicH), graphicW, graphicH);
        x += graphicW + kInnerGap;
    } else if (graphic_) {
        graphic_->performLayout(0, 0, 0, 0);
    }

    double closeW = 0;
    if (close_ && close_->isVisible()) {
        closeW = std::max(0.0, close_->measuredWidth(boxH));
        if (closeW > boxW) {
            closeW = boxW;
        }
    }
    const double reserve = closeW > 0 ? kInnerGap + closeW : 0;
    const double labelW = std::max(0.0, right - reserve - x);
    if (label_) {
        label_->performLayout(x, top, labelW, boxH);
    }
    if (close_ && close_->isVisible()) {
        const double closeH = std::min(boxH, std::max(closeW, close_->measuredHeight(closeW, boxH)));
        close_->performLayout(right - closeW, top + Center(boxH, closeH), closeW, closeH);
    } else if (close_) {
        close_->performLayout(0, 0, 0, 0);
    }
}

TabPane::TabPane() : impl_(std::make_unique<Impl>()) {
    getClassList().add("tab-pane");
    setBackground(Color::white());
    impl_->bar = std::make_shared<HeaderBar>();
    impl_->bar->setParent(this);
    impl_->tabs.setIndexedAddCallback([this](std::shared_ptr<Tab> tab, std::size_t index) {
        onAdded(std::move(tab), index);
    });
    impl_->tabs.setIndexedRemoveCallback([this](std::shared_ptr<Tab> tab, std::size_t index) {
        onRemoved(std::move(tab), index);
    });
}

TabPane::~TabPane() {
    if (!impl_) {
        return;
    }
    impl_->tabs.setIndexedAddCallback(nullptr);
    impl_->tabs.setIndexedRemoveCallback(nullptr);
    impl_->eventDepth = 1;
    for (const std::shared_ptr<Tab>& tab : impl_->tabs.items()) {
        if (tab) {
            tab->detachQuietly();
        }
    }
    if (impl_->shown && impl_->shown->getParent() == this) {
        impl_->shown->setParent(nullptr);
    }
    impl_->shown.reset();
    for (Impl::Slot& slot : impl_->slots) {
        if (slot.header && slot.header->getParent() == this) {
            slot.header->setParent(nullptr);
        }
    }
    impl_->slots.clear();
    impl_->graveyard.clear();
    if (impl_->bar && impl_->bar->getParent() == this) {
        impl_->bar->setParent(nullptr);
    }
    impl_->bar.reset();
    impl_->tabs.clear();
    impl_.reset();
}

ObservableList<std::shared_ptr<Tab>>& TabPane::tabs() { return impl_->tabs; }

const ObservableList<std::shared_ptr<Tab>>& TabPane::tabs() const { return impl_->tabs; }

std::shared_ptr<TabPane::TabHeader> TabPane::makeHeader(const std::shared_ptr<Tab>& tab) {
    return std::make_shared<TabHeader>(*this, tab);
}

void TabPane::select(std::size_t index) {
    if (!impl_ || index >= impl_->tabs.size()) {
        return;
    }
    selectNow(impl_->tabs[index]);
}

void TabPane::select(const std::shared_ptr<Tab>& tab) { selectNow(tab); }

void TabPane::select(Tab* tab) {
    if (!impl_ || tab == nullptr) {
        return;
    }
    for (const std::shared_ptr<Tab>& item : impl_->tabs.items()) {
        if (item.get() == tab) {
            selectNow(item);
            return;
        }
    }
}

Tab* TabPane::getSelectedTab() const {
    return impl_ && impl_->selected ? impl_->selected.get() : nullptr;
}

std::size_t TabPane::getSelectedIndex() const {
    if (!impl_ || !impl_->selected) {
        return npos;
    }
    const std::vector<std::shared_ptr<Tab>>& items = impl_->tabs.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i] == impl_->selected) {
            return i;
        }
    }
    return npos;
}

void TabPane::setSide(Side side) {
    if (!impl_ || impl_->side == side) {
        return;
    }
    impl_->side = side;
}

Side TabPane::getSide() const { return impl_ ? impl_->side : Side::Top; }

void TabPane::setTabClosingPolicy(TabClosingPolicy policy) {
    if (!impl_ || impl_->policy == policy) {
        return;
    }
    impl_->policy = policy;
    syncAll();
}

TabPane::TabClosingPolicy TabPane::getTabClosingPolicy() const {
    return impl_ ? impl_->policy : TabClosingPolicy::SelectedTab;
}

void TabPane::setTabMinWidth(double value) {
    if (impl_) {
        impl_->tabMinWidth = value;
    }
}

void TabPane::setTabMaxWidth(double value) {
    if (impl_) {
        impl_->tabMaxWidth = value;
    }
}

void TabPane::setTabMinHeight(double value) {
    if (impl_) {
        impl_->tabMinHeight = value;
    }
}

void TabPane::setTabMaxHeight(double value) {
    if (impl_) {
        impl_->tabMaxHeight = value;
    }
}

double TabPane::getTabMinWidth() const { return impl_ ? impl_->tabMinWidth : 0; }
double TabPane::getTabMaxWidth() const { return impl_ ? impl_->tabMaxWidth : kUnlimited; }
double TabPane::getTabMinHeight() const { return impl_ ? impl_->tabMinHeight : 0; }
double TabPane::getTabMaxHeight() const { return impl_ ? impl_->tabMaxHeight : kUnlimited; }

void TabPane::setDisable(bool value) {
    if (!impl_ || impl_->disabled == value) {
        return;
    }
    impl_->disabled = value;
    for (const std::shared_ptr<Tab>& tab : impl_->tabs.items()) {
        if (tab) {
            tab->updateDisabled();
        }
    }
}

bool TabPane::isDisabled() const { return impl_ && impl_->disabled; }

bool TabPane::closeShown(const Tab& tab) const {
    if (!impl_ || !tab.isClosable()) {
        return false;
    }
    switch (impl_->policy) {
        case TabClosingPolicy::Unavailable:
            return false;
        case TabClosingPolicy::AllTabs:
            return true;
        case TabClosingPolicy::SelectedTab:
            return tab.isSelected();
    }
    return false;
}

void TabPane::onAdded(std::shared_ptr<Tab> tab, std::size_t index) {
    if (!impl_) {
        return;
    }
    if (!tab) {
        impl_->suppressRemove = true;
        impl_->tabs.removeAt(index);
        impl_->suppressRemove = false;
        return;
    }
    for (std::size_t i = 0; i < impl_->tabs.size(); ++i) {
        if (i != index && impl_->tabs[i] == tab) {
            impl_->suppressRemove = true;
            impl_->tabs.removeAt(index);
            impl_->suppressRemove = false;
            return;
        }
    }
    if (tab->getTabPane() != nullptr && tab->getTabPane() != this) {
        tab->getTabPane()->forget(tab.get());
    }
    tab->assignPane(this);
    if (index > impl_->slots.size()) {
        index = impl_->slots.size();
    }
    Impl::Slot slot;
    slot.tab = tab;
    slot.header = makeHeader(tab);
    slot.header->setParent(this);
    impl_->slots.insert(impl_->slots.begin() + static_cast<std::ptrdiff_t>(index), std::move(slot));
    if (!impl_->selected) {
        selectNow(tab);
    } else {
        syncAll();
    }
}

void TabPane::onRemoved(std::shared_ptr<Tab> tab, std::size_t index) {
    if (!impl_ || impl_->suppressRemove) {
        return;
    }
    const bool owned = tab && tab->getTabPane() == this;
    std::size_t slotIndex = impl_->slots.size();
    if (index < impl_->slots.size() && impl_->slots[index].tab == tab) {
        slotIndex = index;
    } else {
        for (std::size_t i = 0; i < impl_->slots.size(); ++i) {
            if (impl_->slots[i].tab == tab) {
                slotIndex = i;
                break;
            }
        }
    }
    if (slotIndex < impl_->slots.size()) {
        std::shared_ptr<TabHeader> header = std::move(impl_->slots[slotIndex].header);
        impl_->slots.erase(impl_->slots.begin() + static_cast<std::ptrdiff_t>(slotIndex));
        if (header) {
            if (header->getParent() == this) {
                header->setParent(nullptr);
            }
            impl_->graveyard.push_back(std::move(header));
        }
    }
    if (!owned) {
        showSelected();
        return;
    }
    const bool wasSelected = tab->isSelected();
    if (impl_->selected == tab) {
        impl_->selected.reset();
    }
    const std::shared_ptr<Node>& content = tab->contentNode();
    if (content && content->getParent() == this) {
        content->setParent(nullptr);
    }
    if (impl_->shown == content) {
        impl_->shown.reset();
    }
    tab->setSelectedFlag(false);
    tab->assignPane(nullptr);
    if (wasSelected) {
        tab->notifySelection();
    }
    if (wasSelected && !impl_->selected && !impl_->tabs.empty()) {
        selectNow(impl_->tabs[std::min(index, impl_->tabs.size() - 1)]);
    } else {
        showSelected();
        syncAll();
    }
}

void TabPane::showSelected() {
    if (!impl_) {
        return;
    }
    std::shared_ptr<Node> next;
    if (impl_->selected) {
        next = impl_->selected->contentNode();
    }
    if (next && WouldCycle(next.get(), this)) {
        next.reset();
    }
    if (impl_->shown == next) {
        return;
    }
    if (impl_->shown && impl_->shown->getParent() == this) {
        impl_->shown->setParent(nullptr);
    }
    impl_->shown.reset();
    if (!next) {
        return;
    }
    if (next->getParent() == this) {
        impl_->shown = std::move(next);
        return;
    }
    if (next->getParent() != nullptr) {
        next->getParent()->detachChild(next.get());
    }
    if (!next || (impl_->selected && impl_->selected->contentNode() != next)) {
        return;
    }
    next->setParent(this);
    impl_->shown = std::move(next);
}

void TabPane::syncAll() {
    if (!impl_ || impl_->syncing) {
        return;
    }
    impl_->syncing = true;
    if (impl_->bar) {
        impl_->bar->setVisible(!impl_->slots.empty());
    }
    for (Impl::Slot& slot : impl_->slots) {
        if (!slot.tab || !slot.header) {
            continue;
        }
        HeaderState state;
        state.text = slot.tab->getText();
        state.id = slot.tab->getId();
        state.style = slot.tab->getStyle();
        state.classes.push_back("tab");
        for (const std::string& name : slot.tab->getStyleClass().items()) {
            if (name != "tab") {
                state.classes.push_back(name);
            }
        }
        state.graphic = slot.tab->graphicNode();
        state.selected = slot.tab->isSelected();
        state.disabled = slot.tab->isDisabled();
        state.showClose = closeShown(*slot.tab);
        slot.header->apply(std::move(state));
    }
    impl_->syncing = false;
}

void TabPane::requestClose(const std::shared_ptr<Tab>& tab) {
    if (!impl_ || !tab) {
        return;
    }
    if (impl_->closing) {
        impl_->pendingClose = tab;
        return;
    }
    if (tab->getTabPane() != this || tab->isDisabled() || !closeShown(*tab)) {
        return;
    }
    if (tab->notifyCloseRequest()) {
        return;
    }
    impl_->closing = true;
    impl_->tabs.removeIf([&](const std::shared_ptr<Tab>& item) { return item == tab; });
    tab->notifyClosed();
    impl_->closing = false;
    if (impl_->pendingClose) {
        std::shared_ptr<Tab> next = std::move(impl_->pendingClose);
        requestClose(next);
    }
}

void TabPane::forget(Tab* tab) {
    if (!impl_ || tab == nullptr) {
        return;
    }
    impl_->tabs.removeIf([&](const std::shared_ptr<Tab>& item) { return item.get() == tab; });
}

void TabPane::noteContent(Tab& tab, const std::shared_ptr<Node>& previous) {
    if (!impl_) {
        return;
    }
    if (previous && previous->getParent() == this && tab.contentNode() != previous) {
        previous->setParent(nullptr);
        if (impl_->shown == previous) {
            impl_->shown.reset();
        }
    }
    if (impl_->selected.get() == &tab) {
        showSelected();
    }
}

void TabPane::releaseGraphic(Node* child) {
    if (!impl_ || child == nullptr) {
        return;
    }
    for (const std::shared_ptr<Tab>& tab : impl_->tabs.items()) {
        if (tab && tab->graphicNode().get() == child) {
            tab->clearGraphic(child);
        }
    }
}

void TabPane::selectNow(const std::shared_ptr<Tab>& tab) {
    if (!impl_ || !tab || tab->getTabPane() != this || impl_->selected == tab) {
        return;
    }
    std::shared_ptr<Tab> previous = impl_->selected;
    impl_->selected = tab;
    if (previous) {
        previous->setSelectedFlag(false);
    }
    tab->setSelectedFlag(true);
    showSelected();
    syncAll();
    if (previous) {
        previous->notifySelection();
    }
    if (impl_->selected == tab) {
        tab->notifySelection();
    }
}

void TabPane::visitChildren(const std::function<void(Node*)>& visitor) {
    if (!impl_) {
        return;
    }
    if (impl_->bar) {
        visitor(impl_->bar.get());
    }
    for (const Impl::Slot& slot : impl_->slots) {
        if (slot.header) {
            visitor(slot.header.get());
        }
    }
    if (impl_->shown) {
        visitor(impl_->shown.get());
    }
}

void TabPane::detachChild(Node* child) {
    if (child == nullptr || !impl_) {
        return;
    }
    if (impl_->shown.get() == child) {
        impl_->shown.reset();
    }
    for (const std::shared_ptr<Tab>& tab : impl_->tabs.items()) {
        if (tab && tab->contentNode().get() == child) {
            if (child->getParent() == this) {
                child->setParent(nullptr);
            }
            tab->clearContent(child);
        }
    }
    if (impl_->bar.get() == child) {
        if (child->getParent() == this) {
            child->setParent(nullptr);
        }
        impl_->bar = std::make_shared<HeaderBar>();
        impl_->bar->setParent(this);
        return;
    }
    for (Impl::Slot& slot : impl_->slots) {
        if (slot.header.get() != child) {
            continue;
        }
        if (child->getParent() == this) {
            child->setParent(nullptr);
        }
        slot.header = makeHeader(slot.tab);
        slot.header->setParent(this);
        syncAll();
        return;
    }
}

double TabPane::preferredContentWidth(double innerAvailable) const {
    if (!impl_) {
        return 0;
    }
    const bool horizontal = HorizontalSide(impl_->side);
    double main = 0;
    double cross = 0;
    std::size_t count = 0;
    for (const Impl::Slot& slot : impl_->slots) {
        if (!slot.header) {
            continue;
        }
        const double width = ClampAxis(slot.header->measuredWidth(innerAvailable), impl_->tabMinWidth, impl_->tabMaxWidth);
        if (horizontal) {
            main += width;
        } else {
            cross = std::max(cross, width);
        }
        ++count;
    }
    if (horizontal && count > 1) {
        main += kTabGap * static_cast<double>(count - 1);
    }
    double contentWidth = 0;
    if (impl_->selected && impl_->selected->contentNode()) {
        const double available = horizontal ? innerAvailable : std::max(0.0, innerAvailable - cross);
        contentWidth = impl_->selected->contentNode()->measuredWidth(available);
    }
    return horizontal ? std::max(main, contentWidth) : cross + contentWidth;
}

double TabPane::preferredContentHeight(double innerWidth) const {
    if (!impl_) {
        return 0;
    }
    const bool horizontal = HorizontalSide(impl_->side);
    double thickness = 0;
    double stack = 0;
    std::size_t count = 0;
    for (const Impl::Slot& slot : impl_->slots) {
        if (!slot.header) {
            continue;
        }
        const double width = ClampAxis(slot.header->measuredWidth(innerWidth), impl_->tabMinWidth, impl_->tabMaxWidth);
        const double height = ClampAxis(slot.header->measuredHeight(width, -1), impl_->tabMinHeight, impl_->tabMaxHeight);
        if (horizontal) {
            thickness = std::max(thickness, height);
        } else {
            thickness = std::max(thickness, width);
            stack += height;
        }
        ++count;
    }
    if (!horizontal && count > 1) {
        stack += kTabGap * static_cast<double>(count - 1);
    }
    double contentHeight = 0;
    if (impl_->selected && impl_->selected->contentNode()) {
        const double contentWidth = horizontal ? innerWidth : std::max(0.0, innerWidth - thickness);
        contentHeight = impl_->selected->contentNode()->measuredHeight(contentWidth, -1);
    }
    return horizontal ? thickness + contentHeight : std::max(stack, contentHeight);
}

void TabPane::layoutChildren() {
    if (!impl_) {
        return;
    }
    if (impl_->eventDepth == 0) {
        impl_->suppressHeader = false;
        impl_->graveyard.clear();
    }
    if (!impl_->bar) {
        impl_->bar = std::make_shared<HeaderBar>();
        impl_->bar->setParent(this);
    }

    const Side side = impl_->side;
    const bool horizontal = HorizontalSide(side);
    const double innerW = contentWidth();
    const double innerH = contentHeight();
    const double originX = contentLeft();
    const double originY = contentTop();

    struct Piece {
        TabHeader* header = nullptr;
        bool selected = false;
        double main = 0;
        double floor = 0;
    };
    std::vector<Piece> pieces;
    double thickness = 0;
    for (const Impl::Slot& slot : impl_->slots) {
        if (!slot.header) {
            continue;
        }
        Piece piece;
        piece.header = slot.header.get();
        piece.selected = impl_->selected && slot.tab == impl_->selected;
        if (horizontal) {
            const double naturalW = piece.header->measuredWidth(innerW);
            const double naturalH = piece.header->measuredHeight(naturalW, innerH);
            thickness = std::max(thickness, ClampAxis(naturalH, impl_->tabMinHeight, impl_->tabMaxHeight));
            piece.main = ClampAxis(naturalW, impl_->tabMinWidth, impl_->tabMaxWidth);
            piece.floor = std::max(0.0, impl_->tabMinWidth);
        } else {
            const double naturalW = piece.header->measuredWidth(innerW);
            const double clampedW = ClampAxis(naturalW, impl_->tabMinWidth, impl_->tabMaxWidth);
            thickness = std::max(thickness, clampedW);
            const double naturalH = piece.header->measuredHeight(clampedW, innerH);
            piece.main = ClampAxis(naturalH, impl_->tabMinHeight, impl_->tabMaxHeight);
            piece.floor = std::max(0.0, impl_->tabMinHeight);
        }
        pieces.push_back(piece);
    }

    std::vector<double> mains;
    std::vector<double> floors;
    mains.reserve(pieces.size());
    floors.reserve(pieces.size());
    for (const Piece& piece : pieces) {
        mains.push_back(piece.main);
        floors.push_back(piece.floor);
    }
    const double gaps = pieces.size() > 1 ? kTabGap * static_cast<double>(pieces.size() - 1) : 0;
    const double availableMain = horizontal ? innerW : innerH;
    FitDown(mains, floors, std::max(0.0, availableMain - gaps));
    if (horizontal) {
        thickness = std::min(thickness, innerH);
    } else {
        thickness = std::min(thickness, innerW);
    }

    double barX = originX;
    double barY = originY;
    double barW = innerW;
    double barH = innerH;
    if (pieces.empty()) {
        thickness = 0;
    }
    if (horizontal) {
        barH = thickness;
        if (side == Side::Bottom) {
            barY = originY + std::max(0.0, innerH - thickness);
        }
    } else {
        barW = thickness;
        if (side == Side::Right) {
            barX = originX + std::max(0.0, innerW - thickness);
        }
    }
    impl_->bar->setVisible(thickness > 0 && !pieces.empty());
    impl_->bar->setEdge(side);
    impl_->bar->performLayout(barX, barY, std::max(0.0, barW), std::max(0.0, barH));

    double cursor = horizontal ? barX : barY;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const double inset = pieces[i].selected ? 0 : std::min(2.0, thickness);
        double x = 0;
        double y = 0;
        double width = 0;
        double height = 0;
        if (horizontal) {
            width = mains[i];
            height = std::max(0.0, thickness - inset);
            x = cursor;
            y = side == Side::Top ? barY : barY + inset;
            cursor += width + kTabGap;
        } else {
            height = mains[i];
            width = std::max(0.0, thickness - inset);
            y = cursor;
            x = side == Side::Left ? barX : barX + inset;
            cursor += height + kTabGap;
        }
        pieces[i].header->performLayout(x, y, width, height);
    }

    double pageX = originX;
    double pageY = originY;
    double pageW = innerW;
    double pageH = innerH;
    if (horizontal) {
        pageH = std::max(0.0, innerH - thickness);
        if (side == Side::Top) {
            pageY = originY + thickness;
        }
    } else {
        pageW = std::max(0.0, innerW - thickness);
        if (side == Side::Left) {
            pageX = originX + thickness;
        }
    }
    if (impl_->shown) {
        const double width = layout_detail::AxisSize(impl_->shown.get(), pageW, true);
        const double height = layout_detail::AxisSize(impl_->shown.get(), pageH, false);
        impl_->shown->performLayout(pageX, pageY, width, height);
    }
}

}  // namespace jadefx
