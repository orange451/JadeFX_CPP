#include "jadefx/scene/controls/TreeView.hpp"

#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/controls/ScrollBar.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jadefx {

class TreeScrollBar : public Region {
public:
    explicit TreeScrollBar(TreeView& view) : view_(&view) {
        setDefaultCursor(Cursor::Default);
        getClassList().add("scroll-bar");
    }

    const char* getElementType() const override { return "scroll-bar"; }

    void clearView() { view_ = nullptr; }

    void handleMousePressed(const MouseEvent& event) override {
        if (view_ != nullptr) {
            view_->pressScrollBar(event);
        }
    }

    void handleMouseDragged(const MouseEvent& event) override {
        if (view_ != nullptr) {
            view_->dragScrollBar(event);
        }
    }

    void handleMouseReleased(const MouseEvent&) override {
        if (view_ != nullptr) {
            view_->releaseScrollBar();
        }
    }

    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    TreeView* view_ = nullptr;
};

namespace {

constexpr double kDisclosure = 18;
constexpr double kBarWidth = 3;
constexpr double kGraphicGap = 3;
constexpr double kRightPad = 10;
constexpr double kDefaultRow = 32;
constexpr double kDefaultIndent = 10;
constexpr double kMinRow = 8;
constexpr double kDoubleClickSeconds = 0.4;
constexpr double kArrowSeconds = 0.16;
// Full-grown diameter, in row heights. The circle is larger than the row and clipped to it.
constexpr float kRippleDiameter = 7.f;

double Now() {
    using Clock = std::chrono::steady_clock;
    static const auto start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

float EaseOut(float t) {
    if (t < 0.f) {
        return 0.f;
    }
    if (t > 1.f) {
        return 1.f;
    }
    const float remain = 1.f - t;
    return 1.f - remain * remain;
}

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

void FillTriangle(UiRenderer& renderer, float x0, float y0, float x1, float y1, float x2, float y2,
                  const Color& color) {
    const float minY = std::min(y0, std::min(y1, y2));
    const float maxY = std::max(y0, std::max(y1, y2));
    const float radius[4] = {};
    const float at = 0.f;
    auto edge = [](float y, float ax, float ay, float bx, float by, float& hit) {
        if (std::fabs(ay - by) < 1e-4f) {
            return false;
        }
        const float low = std::min(ay, by);
        const float high = std::max(ay, by);
        if (y < low || y > high) {
            return false;
        }
        const float t = (y - ay) / (by - ay);
        if (t < 0.f || t > 1.f) {
            return false;
        }
        hit = ax + (bx - ax) * t;
        return true;
    };
    for (float y = std::floor(minY); y < maxY; y += 1.f) {
        const float sample = y + 0.5f;
        float hits[3];
        int count = 0;
        auto add = [&](float ax, float ay, float bx, float by) {
            float hit = 0.f;
            if (count < 3 && edge(sample, ax, ay, bx, by, hit)) {
                hits[count++] = hit;
            }
        };
        add(x0, y0, x1, y1);
        add(x1, y1, x2, y2);
        add(x2, y2, x0, y0);
        if (count < 2) {
            continue;
        }
        float left = hits[0];
        float right = hits[0];
        for (int i = 1; i < count; ++i) {
            left = std::min(left, hits[i]);
            right = std::max(right, hits[i]);
        }
        if (right - left < 0.4f) {
            continue;
        }
        renderer.fillRounded(left, y, right - left, 1.f, radius, &color, &at, 1, 0.f);
    }
}

void DrawArrow(UiRenderer& renderer, float centerX, float centerY, float size, float angleDeg, const Color& color) {
    if (color.a <= 0.f || size <= 0.f) {
        return;
    }
    const float rad = angleDeg * 0.0174532925f;
    const float cs = std::cos(rad);
    const float sn = std::sin(rad);
    const float halfW = size * 0.34f;
    const float halfH = size * 0.42f;
    const float local[3][2] = {{-halfW, -halfH}, {-halfW, halfH}, {halfW, 0.f}};
    float x[3];
    float y[3];
    for (int i = 0; i < 3; ++i) {
        x[i] = centerX + local[i][0] * cs - local[i][1] * sn;
        y[i] = centerY + local[i][0] * sn + local[i][1] * cs;
    }
    FillTriangle(renderer, x[0], y[0], x[1], y[1], x[2], y[2], color);
}

class TreeCellLabel : public Label {
public:
    explicit TreeCellLabel(std::string text) : Label(std::move(text)) {
        getClassList().add("tree-cell-label");
        setAlignment(Pos::CenterLeft);
        setMouseTransparent(true);
    }

    const char* getElementType() const override { return "tree-cell-label"; }
};

class SelectionBar : public Region {
public:
    SelectionBar() {
        getClassList().add("selection-bar");
        setMouseTransparent(true);
        setVisible(false);
    }

    const char* getElementType() const override { return "selection-bar"; }
};

class TreeDisclosure : public Region {
public:
    TreeDisclosure() {
        setDefaultCursor(Cursor::Pointer);
        getClassList().add("tree-disclosure-node");
    }

    const char* getElementType() const override { return "tree-disclosure-node"; }

    void setOpen(bool open, bool animate) {
        const float next = open ? 90.f : 0.f;
        if (!animate) {
            from_ = next;
            to_ = next;
            shown_ = next;
            return;
        }
        if (std::fabs(to_ - next) < 0.1f && std::fabs(shown_ - next) < 0.1f) {
            return;
        }
        from_ = shown_;
        to_ = next;
        start_ = Now();
    }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 1.f || height <= 1.f) {
            return;
        }
        const double elapsed = start_ > 0 ? Now() - start_ : kArrowSeconds;
        const float t = EaseOut(static_cast<float>(elapsed / kArrowSeconds));
        shown_ = from_ + (to_ - from_) * t;
        Color color = computedStyle().color;
        color.a *= opacity;
        const float size = std::min(width, height) * 0.55f;
        DrawArrow(renderer, static_cast<float>(getAbsoluteX() + getWidth() * 0.5),
                  static_cast<float>(getAbsoluteY() + getHeight() * 0.5), size, shown_, color);
    }

private:
    float from_ = 0.f;
    float to_ = 0.f;
    float shown_ = 0.f;
    double start_ = 0;
};

class TreeRipple : public Region {
public:
    explicit TreeRipple(class TreeCell& cell) : cell_(&cell) { setMouseTransparent(true); }

    const char* getElementType() const override { return "tree-ripple"; }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    TreeCell* cell_ = nullptr;
};

class TreeCell : public Region {
public:
    explicit TreeCell(TreeView& view) : view_(&view) {
        setDefaultCursor(Cursor::Pointer);
        getClassList().add("tree-cell");
        bar_ = std::make_shared<SelectionBar>();
        bar_->setParent(this);
        ripple_ = std::make_shared<TreeRipple>(*this);
        ripple_->setParent(this);
        disclosure_ = std::make_shared<TreeDisclosure>();
        disclosure_->setParent(this);
        label_ = std::make_shared<TreeCellLabel>("");
        label_->setParent(this);

        disclosure_->setOnMouseClicked([this](const MouseEvent&) { clickedArrow(); });
        setOnMousePressed([this](const MouseEvent& event) { pressed(event); });
        setOnMouseReleased([this](const MouseEvent&) { released(); });
        setOnMouseClicked([this](const MouseEvent&) { clickedRow(); });
        setOnContextMenuRequested([this](const MouseEvent& event) { contextMenu(event); });
        setOnMouseEntered([this](const MouseEvent&) { updateChrome(); });
        setOnMouseExited([this](const MouseEvent&) { updateChrome(); });
    }

    ~TreeCell() override {
        view_ = nullptr;
        item_ = nullptr;
        release(graphic_);
        release(label_);
        release(disclosure_);
        release(bar_);
        release(ripple_);
    }

    const char* getElementType() const override { return "tree-cell"; }

    TreeItem* item() const { return item_; }

    void detachView() {
        view_ = nullptr;
        item_ = nullptr;
    }

    void bind(TreeItem* item) {
        const bool same = item_ == item;
        const bool open = item != nullptr && item->isExpanded() && !item->isLeaf();
        if (disclosure_) {
            disclosure_->setOpen(open, same);
        }
        item_ = item;
        if (label_) {
            label_->setText(item != nullptr ? item->getValue() : std::string());
        }
        adoptGraphic(item != nullptr ? item->getGraphic() : nullptr);
        setExpandedClass(open);
        updateChrome();
    }

    void prepare(int level, double indent) {
        level_ = level;
        indent_ = indent;
    }

    void updateChrome() {
        const bool selected = view_ != nullptr && item_ != nullptr && view_->getSelectedItem() == item_;
        setSelected(selected);
        if (bar_) {
            if (view_ != nullptr) {
                bar_->setBackground(view_->getSelectionBarColor());
            }
            bar_->setVisible(selected);
        }
        if (selected && isHovered()) {
            setBackground(Color::rgb8(210, 227, 252));
        } else if (selected) {
            setBackground(Color::rgb8(232, 240, 254));
        } else if (isHovered()) {
            setBackground(Color::rgb8(245, 245, 245));
        } else {
            setBackground(Color::transparent());
        }
    }

    void drawRipple(UiRenderer& renderer, float opacity) const {
        if (!rippling_ || getWidth() <= 1 || getHeight() <= 1) {
            return;
        }
        const double elapsed = Now() - rippleStart_;
        const float grow = EaseOut(static_cast<float>(elapsed / 0.28));
        float alpha = 0.16f * (1.f - 0.35f * grow);
        if (rippleRelease_ >= 0) {
            const float fade = static_cast<float>((Now() - rippleRelease_) / 0.18);
            if (fade >= 1.f) {
                return;
            }
            alpha *= 1.f - fade;
        }
        if (alpha <= 0.f) {
            return;
        }
        const float height = static_cast<float>(getHeight());
        const float width = static_cast<float>(getWidth());
        const float radius = height * (kRippleDiameter * 0.5f) * std::max(grow, 0.08f);
        const float cx = rippleX_;
        const float cy = rippleY_;
        const float box = radius * 2.f;
        const float corner[4] = {radius, radius, radius, radius};
        const float at = 0.f;
        Color color = Color::rgba(0.f, 0.f, 0.f, alpha * opacity);
        const float absX = static_cast<float>(getAbsoluteX());
        const float absY = static_cast<float>(getAbsoluteY());
        renderer.pushClip(absX, absY, width, height);
        renderer.fillRounded(absX + cx - radius, absY + cy - radius, box, box, corner, &color, &at, 1, 0.f);
        renderer.popClip();
    }

    void detachChild(Node* child) override {
        if (child == nullptr) {
            return;
        }
        if (graphic_.get() == child) {
            std::shared_ptr<Node> lost = std::move(graphic_);
            if (lost && lost->getParent() == this) {
                lost->setParent(nullptr);
            }
            if (item_ != nullptr && item_->getGraphic() == lost) {
                item_->setGraphic(nullptr);
            }
            return;
        }
        if (label_.get() == child) {
            if (label_->getParent() == this) {
                label_->setParent(nullptr);
            }
            label_.reset();
            label_ = std::make_shared<TreeCellLabel>(item_ != nullptr ? item_->getValue() : std::string());
            label_->setParent(this);
            return;
        }
        if (disclosure_.get() == child) {
            if (disclosure_->getParent() == this) {
                disclosure_->setParent(nullptr);
            }
            disclosure_ = std::make_shared<TreeDisclosure>();
            disclosure_->setParent(this);
            disclosure_->setOnMouseClicked([this](const MouseEvent&) { clickedArrow(); });
            return;
        }
    }

protected:
    void layoutChildren() override {
        const double width = getWidth();
        const double height = getHeight();
        if (bar_) {
            if (bar_->isVisible()) {
                bar_->performLayout(0, 0, kBarWidth, height);
            } else {
                bar_->performLayout(0, 0, 0, 0);
            }
        }
        if (ripple_) {
            ripple_->performLayout(0, 0, width, height);
        }
        const bool branch = item_ != nullptr && !item_->isLeaf();
        const double arrowX = std::max(0.0, indent_) * std::max(0, level_);
        if (disclosure_) {
            if (branch && height > 0) {
                disclosure_->setVisible(true);
                const double arrowH = std::min(kDisclosure, height);
                disclosure_->performLayout(arrowX, (height - arrowH) * 0.5, kDisclosure, arrowH);
            } else {
                disclosure_->setVisible(false);
                disclosure_->performLayout(0, 0, 0, 0);
            }
        }
        double x = arrowX + kDisclosure;
        const double limit = std::max(x, width - kRightPad);
        if (graphic_ && graphic_->isVisible()) {
            const double avail = std::max(0.0, limit - x - kGraphicGap);
            const double graphicW = std::min(avail, std::max(0.0, graphic_->measuredWidth(avail)));
            const double graphicH = std::min(height, std::max(0.0, graphic_->measuredHeight(graphicW, height)));
            graphic_->performLayout(x, (height - graphicH) * 0.5, graphicW, graphicH);
            x += graphicW + kGraphicGap;
        } else if (graphic_) {
            graphic_->performLayout(0, 0, 0, 0);
        }
        if (label_) {
            label_->performLayout(x, 0, std::max(0.0, limit - x), height);
        }
    }

    void visitChildren(const std::function<void(Node*)>& visitor) override {
        if (bar_) {
            visitor(bar_.get());
        }
        if (ripple_) {
            visitor(ripple_.get());
        }
        if (disclosure_) {
            visitor(disclosure_.get());
        }
        if (graphic_) {
            visitor(graphic_.get());
        }
        if (label_) {
            visitor(label_.get());
        }
    }

private:
    template <typename T>
    void release(std::shared_ptr<T>& node) {
        if (node && node->getParent() == this) {
            node->setParent(nullptr);
        }
        node.reset();
    }

    void adoptGraphic(std::shared_ptr<Node> graphic) {
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
        release(graphic_);
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

    void setExpandedClass(bool open) {
        bool has = false;
        for (const std::string& name : getClassList().items()) {
            if (name == "expanded") {
                has = true;
            }
        }
        if (open && !has) {
            getClassList().add("expanded");
        } else if (!open && has) {
            getClassList().removeIf([](const std::string& name) { return name == "expanded"; });
        }
    }

    void pressed(const MouseEvent& event) {
        rippleX_ = static_cast<float>(event.x - getAbsoluteX());
        rippleY_ = static_cast<float>(event.y - getAbsoluteY());
        rippleStart_ = Now();
        rippleRelease_ = -1;
        rippling_ = true;
    }

    void released() {
        if (rippling_) {
            rippleRelease_ = Now();
        }
    }

    void clickedArrow() {
        arrowClick_ = true;
        TreeItem* item = item_;
        TreeView* view = view_;
        if (item == nullptr || view == nullptr || item->isLeaf()) {
            return;
        }
        view->select(item);
        item->setExpanded(!item->isExpanded());
        lastClick_ = 0;
    }

    void clickedRow() {
        if (arrowClick_) {
            arrowClick_ = false;
            return;
        }
        TreeItem* item = item_;
        TreeView* view = view_;
        if (item == nullptr || view == nullptr) {
            return;
        }
        const double now = Now();
        const bool repeat = lastClick_ > 0 && now - lastClick_ < kDoubleClickSeconds;
        lastClick_ = repeat ? 0 : now;
        view->select(item);
        if (repeat) {
            const bool handled = view->itemActivated(*item);
            if (!handled && !item->isLeaf()) {
                item->setExpanded(!item->isExpanded());
            }
        }
    }

    void contextMenu(const MouseEvent& event) {
        TreeItem* item = item_;
        TreeView* view = view_;
        if (item == nullptr || view == nullptr) {
            return;
        }
        view->select(item);
        view->contextMenuRequested(*item, event);
    }

    TreeView* view_ = nullptr;
    TreeItem* item_ = nullptr;
    std::shared_ptr<SelectionBar> bar_;
    std::shared_ptr<TreeRipple> ripple_;
    std::shared_ptr<TreeDisclosure> disclosure_;
    std::shared_ptr<TreeCellLabel> label_;
    std::shared_ptr<Node> graphic_;
    int level_ = 0;
    double indent_ = kDefaultIndent;
    bool arrowClick_ = false;
    double lastClick_ = 0;
    bool rippling_ = false;
    double rippleStart_ = 0;
    double rippleRelease_ = -1;
    float rippleX_ = 0.f;
    float rippleY_ = 0.f;

    friend class TreeRipple;
};

void TreeRipple::renderContent(UiRenderer& renderer, float opacity) {
    if (cell_ != nullptr) {
        cell_->drawRipple(renderer, opacity);
    }
}

std::shared_ptr<TreeItem> FindShared(const std::shared_ptr<TreeItem>& node, const TreeItem* target) {
    if (!node || target == nullptr) {
        return nullptr;
    }
    if (node.get() == target) {
        return node;
    }
    for (const std::shared_ptr<TreeItem>& child : node->getChildren().items()) {
        if (std::shared_ptr<TreeItem> found = FindShared(child, target)) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

struct TreeView::Impl {
    std::shared_ptr<TreeItem> root;
    TreeItem* watched = nullptr;
    std::vector<std::shared_ptr<TreeCell>> rows;
    std::shared_ptr<TreeItem> selected;
    std::function<void(TreeItem*)> onSelection;
    std::function<void(TreeItem&, const MouseEvent&)> onContext;
    std::function<bool(TreeItem&)> onActivated;
    bool showRoot = true;
    double indent = kDefaultIndent;
    double cellSize = kDefaultRow;
    Color barColor = Color::rgb8(26, 115, 232);
    std::shared_ptr<TreeScrollBar> track;
    ScrollBar vbar;
    double scroll = 0;
    bool scrollDrag = false;
    float scrollGrab = 0.f;
    int visibleRows = 1;
    int syncDepth = 0;
    int selectDepth = 0;
    bool syncAgain = false;
    bool alive = true;
};

TreeView::TreeView() : impl_(std::make_unique<Impl>()) {
    getClassList().add("tree-view");
    setBackground(Color::white());
    impl_->track = std::make_shared<TreeScrollBar>(*this);
    impl_->track->setParent(this);
}

TreeView::TreeView(std::shared_ptr<TreeItem> root) : TreeView() { setRoot(std::move(root)); }

TreeView::~TreeView() {
    if (!impl_) {
        return;
    }
    impl_->alive = false;
    impl_->onSelection = nullptr;
    impl_->onContext = nullptr;
    impl_->onActivated = nullptr;
    if (impl_->watched != nullptr) {
        impl_->watched->setStructureListener(nullptr);
        impl_->watched = nullptr;
    }
    for (const std::shared_ptr<TreeCell>& row : impl_->rows) {
        if (!row) {
            continue;
        }
        row->detachView();
        if (row->getParent() == this) {
            row->setParent(nullptr);
        }
    }
    impl_->rows.clear();
    if (impl_->track) {
        impl_->track->clearView();
        if (impl_->track->getParent() == this) {
            impl_->track->setParent(nullptr);
        }
        impl_->track.reset();
    }
    impl_->root.reset();
    impl_.reset();
}

void TreeView::setRoot(std::shared_ptr<TreeItem> root) {
    if (!impl_ || !impl_->alive) {
        return;
    }
    if (impl_->root == root) {
        return;
    }
    if (impl_->watched != nullptr) {
        impl_->watched->setStructureListener(nullptr);
        impl_->watched = nullptr;
    }
    impl_->root = std::move(root);
    watchRoot();
    requestSync();
}

std::shared_ptr<TreeItem> TreeView::getRoot() const { return impl_ ? impl_->root : nullptr; }

void TreeView::setShowRoot(bool show) {
    if (!impl_ || impl_->showRoot == show) {
        return;
    }
    impl_->showRoot = show;
    requestSync();
}

bool TreeView::isShowRoot() const { return !impl_ || impl_->showRoot; }

void TreeView::setIndent(double indent) {
    if (!impl_) {
        return;
    }
    if (indent < 0) {
        indent = 0;
    }
    impl_->indent = indent;
}

double TreeView::getIndent() const { return impl_ ? impl_->indent : kDefaultIndent; }

void TreeView::setFixedCellSize(double size) {
    if (!impl_) {
        return;
    }
    impl_->cellSize = size < kMinRow ? kMinRow : size;
}

double TreeView::getFixedCellSize() const { return impl_ ? impl_->cellSize : kDefaultRow; }

void TreeView::setSelectionBarColor(const Color& color) {
    if (!impl_) {
        return;
    }
    impl_->barColor = color;
    refreshChrome();
}

Color TreeView::getSelectionBarColor() const {
    return impl_ ? impl_->barColor : Color::rgb8(26, 115, 232);
}

int TreeView::getExpandedItemCount() const {
    int count = 0;
    eachVisible([&](TreeItem*) { ++count; });
    return count;
}

TreeItem* TreeView::getTreeItem(int row) const {
    if (row < 0) {
        return nullptr;
    }
    TreeItem* found = nullptr;
    int index = 0;
    eachVisible([&](TreeItem* item) {
        if (index == row) {
            found = item;
        }
        ++index;
    });
    return found;
}

int TreeView::getRow(const TreeItem* item) const {
    if (item == nullptr) {
        return -1;
    }
    int found = -1;
    int index = 0;
    eachVisible([&](TreeItem* cursor) {
        if (cursor == item) {
            found = index;
        }
        ++index;
    });
    return found;
}

TreeItem* TreeView::getSelectedItem() const {
    return impl_ && impl_->selected ? impl_->selected.get() : nullptr;
}

int TreeView::getSelectedIndex() const { return getRow(getSelectedItem()); }

void TreeView::select(TreeItem* item) { selectPointer(item); }

void TreeView::select(int row) {
    TreeItem* item = getTreeItem(row);
    if (item != nullptr) {
        selectPointer(item);
    }
}

void TreeView::clearSelection() { selectPointer(nullptr); }

void TreeView::setOnSelectionChanged(std::function<void(TreeItem*)> handler) {
    if (impl_) {
        impl_->onSelection = std::move(handler);
    }
}

void TreeView::setOnContextMenuRequested(std::function<void(TreeItem&, const MouseEvent&)> handler) {
    if (impl_) {
        impl_->onContext = std::move(handler);
    }
}

void TreeView::setOnItemActivated(std::function<bool(TreeItem&)> handler) {
    if (impl_) {
        impl_->onActivated = std::move(handler);
    }
}

void TreeView::contextMenuRequested(TreeItem& item, const MouseEvent& event) {
    if (!impl_ || !impl_->alive) {
        return;
    }
    select(&item);
    if (impl_->onContext) {
        impl_->onContext(item, event);
    }
}

bool TreeView::itemActivated(TreeItem& item) {
    if (!impl_ || !impl_->onActivated) {
        return false;
    }
    return impl_->onActivated(item);
}

void TreeView::scrollTo(int row) {
    if (!impl_) {
        return;
    }
    if (row < 0) {
        row = 0;
    }
    impl_->scroll = static_cast<double>(row) * rowSize();
}

void TreeView::scrollTo(const TreeItem* item) {
    if (!impl_ || item == nullptr || !containsItem(item)) {
        return;
    }
    std::vector<TreeItem*> chain;
    for (TreeItem* cursor = const_cast<TreeItem*>(item); cursor != nullptr; cursor = cursor->getParent()) {
        chain.push_back(cursor);
    }
    for (std::size_t i = chain.size(); i-- > 1;) {
        if (!chain[i]->isExpanded()) {
            chain[i]->setExpanded(true);
        }
    }
    const int row = getRow(item);
    if (row >= 0) {
        scrollTo(row);
    }
}

void TreeView::watchRoot() {
    if (!impl_ || !impl_->root) {
        return;
    }
    impl_->watched = impl_->root.get();
    impl_->root->setStructureListener([this] { requestSync(); });
}

void TreeView::requestSync() {
    if (!impl_ || !impl_->alive) {
        return;
    }
    if (impl_->syncDepth > 0) {
        impl_->syncAgain = true;
        return;
    }
    ++impl_->syncDepth;
    int guard = 0;
    do {
        impl_->syncAgain = false;
        rebuild();
    } while (impl_->syncAgain && ++guard < 6);
    --impl_->syncDepth;
    if (!impl_->alive) {
        return;
    }
    if (impl_->selected && !containsItem(impl_->selected.get())) {
        impl_->selected.reset();
        refreshChrome();
        if (impl_->onSelection && impl_->selectDepth == 0) {
            ++impl_->selectDepth;
            impl_->onSelection(nullptr);
            --impl_->selectDepth;
        }
    }
}

void TreeView::rebuild() {
    if (!impl_ || !impl_->alive) {
        return;
    }
    std::unordered_map<TreeItem*, std::shared_ptr<TreeCell>> pool;
    for (const std::shared_ptr<TreeCell>& row : impl_->rows) {
        if (row && row->item() != nullptr) {
            pool[row->item()] = row;
        }
    }
    std::vector<std::shared_ptr<TreeCell>> next;
    eachVisible([&](TreeItem* item) {
        std::shared_ptr<TreeCell> cell;
        const auto found = pool.find(item);
        if (found != pool.end()) {
            cell = found->second;
            pool.erase(found);
        } else {
            cell = std::make_shared<TreeCell>(*this);
            cell->setParent(this);
        }
        cell->bind(item);
        next.push_back(std::move(cell));
    });
    for (const auto& entry : pool) {
        if (!entry.second) {
            continue;
        }
        entry.second->detachView();
        if (entry.second->getParent() == this) {
            entry.second->setParent(nullptr);
        }
    }
    impl_->rows = std::move(next);
}

void TreeView::refreshChrome() {
    if (!impl_) {
        return;
    }
    for (const std::shared_ptr<TreeCell>& row : impl_->rows) {
        if (row) {
            row->updateChrome();
        }
    }
}

void TreeView::handleScroll(ScrollEvent& event) {
    if (!impl_ || !impl_->alive) {
        return;
    }
    const double view = contentHeight();
    const double content = static_cast<double>(getExpandedItemCount()) * rowSize();
    if (view <= 0.5 || view + 0.5 >= content) {
        return;
    }
    // A mouse notch is about 1. A trackpad sends many smaller steps. Both move
    // the rows by that fraction of a row, and the next layout keeps the remainder.
    impl_->scroll -= event.deltaY * rowSize();
    event.consume();
}

void TreeView::handleKey(KeyEvent& event) {
    if (!impl_ || !impl_->alive || !event.pressed || impl_->root == nullptr) {
        return;
    }
    const int count = getExpandedItemCount();
    if (count <= 0) {
        return;
    }
    TreeItem* item = getSelectedItem();
    if (event.key == Key::Down) {
        moveSelection(1);
    } else if (event.key == Key::Up) {
        moveSelection(-1);
    } else if (event.key == Key::Home) {
        select(0);
        revealRow(0);
    } else if (event.key == Key::End) {
        select(count - 1);
        revealRow(count - 1);
    } else if (event.key == Key::PageDown) {
        moveSelection(std::max(1, impl_->visibleRows));
    } else if (event.key == Key::PageUp) {
        moveSelection(-std::max(1, impl_->visibleRows));
    } else if (event.key == Key::Right) {
        if (item == nullptr) {
            select(0);
            revealRow(0);
        } else if (!item->isLeaf() && !item->isExpanded()) {
            item->setExpanded(true);
        } else if (!item->isLeaf()) {
            for (const std::shared_ptr<TreeItem>& child : item->getChildren().items()) {
                if (!child) {
                    continue;
                }
                select(child.get());
                revealRow(getRow(child.get()));
                break;
            }
        }
    } else if (event.key == Key::Left || event.key == Key::Enter || event.key == Key::Space) {
        const bool toggle = event.key == Key::Enter || event.key == Key::Space;
        if (item == nullptr) {
            select(0);
            revealRow(0);
        } else if (toggle) {
            if (!item->isLeaf()) {
                item->setExpanded(!item->isExpanded());
            }
        } else if (!item->isLeaf() && item->isExpanded()) {
            item->setExpanded(false);
        } else if (TreeItem* parent = item->getParent()) {
            const bool parentVisible = parent != impl_->root.get() || impl_->showRoot;
            if (parentVisible) {
                select(parent);
                revealRow(getRow(parent));
            } else if (impl_->root && impl_->root->isExpanded()) {
                impl_->root->setExpanded(false);
            }
        }
    } else {
        return;
    }
    event.consume();
}

void TreeView::moveSelection(int delta) {
    const int count = getExpandedItemCount();
    if (count <= 0) {
        return;
    }
    int row = getSelectedIndex();
    if (row < 0) {
        row = delta < 0 ? count - 1 : 0;
    } else {
        row += delta;
        if (row < 0) {
            row = 0;
        }
        if (row >= count) {
            row = count - 1;
        }
    }
    select(row);
    revealRow(row);
}

void TreeView::revealRow(int row) {
    if (!impl_ || row < 0) {
        return;
    }
    const double rowH = rowSize();
    const double rowTop = static_cast<double>(row) * rowH;
    const double view = contentHeight();
    const double page = view > rowH ? view : static_cast<double>(std::max(1, impl_->visibleRows)) * rowH;
    if (rowTop < impl_->scroll) {
        impl_->scroll = rowTop;
    } else if (rowTop + rowH > impl_->scroll + page) {
        impl_->scroll = rowTop + rowH - page;
    }
}

void TreeView::selectPointer(TreeItem* item) {
    if (!impl_ || !impl_->alive || impl_->selectDepth > 3) {
        return;
    }
    std::shared_ptr<TreeItem> next;
    if (item != nullptr) {
        next = findShared(item);
        if (!next) {
            return;
        }
    }
    if (impl_->selected == next) {
        refreshChrome();
        return;
    }
    impl_->selected = std::move(next);
    refreshChrome();
    if (impl_->onSelection) {
        ++impl_->selectDepth;
        impl_->onSelection(impl_->selected.get());
        --impl_->selectDepth;
    }
}

bool TreeView::containsItem(const TreeItem* item) const {
    if (item == nullptr || !impl_ || !impl_->root) {
        return false;
    }
    if (item == impl_->root.get()) {
        return true;
    }
    bool found = false;
    std::function<void(const TreeItem*)> walk = [&](const TreeItem* node) {
        if (node == nullptr || found) {
            return;
        }
        for (const std::shared_ptr<TreeItem>& child : node->getChildren().items()) {
            if (!child || found) {
                continue;
            }
            if (child.get() == item) {
                found = true;
                return;
            }
            walk(child.get());
        }
    };
    walk(impl_->root.get());
    return found;
}

int TreeView::shownLevel(const TreeItem* item) const {
    if (item == nullptr || !impl_ || !impl_->root) {
        return 0;
    }
    int level = 0;
    for (const TreeItem* cursor = item; cursor != nullptr && cursor != impl_->root.get(); cursor = cursor->getParent()) {
        ++level;
    }
    if (!impl_->showRoot) {
        --level;
    }
    return level < 0 ? 0 : level;
}

double TreeView::rowSize() const {
    if (!impl_ || impl_->cellSize < kMinRow) {
        return kDefaultRow;
    }
    return impl_->cellSize;
}

void TreeView::eachVisible(const std::function<void(TreeItem*)>& visit) const {
    if (!impl_ || !impl_->root || !visit) {
        return;
    }
    const bool showRoot = impl_->showRoot;
    const TreeItem* root = impl_->root.get();
    std::function<void(TreeItem*)> walk = [&](TreeItem* item) {
        if (item == nullptr) {
            return;
        }
        if (item != root || showRoot) {
            visit(item);
        }
        if (!item->isExpanded()) {
            return;
        }
        for (const std::shared_ptr<TreeItem>& child : item->getChildren().items()) {
            if (child) {
                walk(child.get());
            }
        }
    };
    walk(impl_->root.get());
}

std::shared_ptr<TreeItem> TreeView::findShared(const TreeItem* item) const {
    if (!impl_) {
        return nullptr;
    }
    return FindShared(impl_->root, item);
}

void TreeView::layoutChildren() {
    if (!impl_) {
        return;
    }
    const double row = rowSize();
    const double width = contentWidth();
    const double height = contentHeight();
    const double top = contentTop();
    const double left = contentLeft();
    const int count = static_cast<int>(impl_->rows.size());
    int fit = static_cast<int>(std::floor((height + 0.01) / row));
    if (fit < 1) {
        fit = 1;
    }
    const double content = static_cast<double>(count) * row;
    double maxScroll = std::max(0.0, content - height);
    if (height + 0.01 >= content) {
        maxScroll = 0;
    }
    if (impl_->scroll < 0) {
        impl_->scroll = 0;
    }
    if (impl_->scroll > maxScroll) {
        impl_->scroll = maxScroll;
    }
    const double scroll = impl_->scroll;
    impl_->vbar = ScrollBar::vertical(static_cast<float>(left + width - ScrollBar::kThickness), static_cast<float>(top),
                                       static_cast<float>(height), static_cast<float>(content), static_cast<float>(height),
                                       scroll);
    impl_->visibleRows = fit;
    const double gutter = impl_->vbar.visible ? ScrollBar::kThickness : 0.0;
    const double rowWidth = std::max(0.0, width - gutter);
    const double viewBottom = top + height;
    for (int i = 0; i < count; ++i) {
        TreeCell* cell = impl_->rows[static_cast<std::size_t>(i)].get();
        if (cell == nullptr) {
            continue;
        }
        cell->prepare(shownLevel(cell->item()), impl_->indent);
        cell->updateChrome();
        const double y = top + static_cast<double>(i) * row - scroll;
        const bool onScreen = height > 0.5 && y + row > top + 0.5 && y < viewBottom - 0.5;
        if (!onScreen) {
            cell->setVisible(false);
            cell->performLayout(0, 0, 0, 0);
            continue;
        }
        cell->setVisible(true);
        cell->performLayout(left, y, rowWidth, row);
    }
    if (impl_->track) {
        if (impl_->vbar.visible) {
            const double x = std::max(left, static_cast<double>(impl_->vbar.cross) - ScrollBar::kHitSlop);
            const double right = std::min(left + width, static_cast<double>(impl_->vbar.cross + impl_->vbar.thickness));
            impl_->track->setVisible(true);
            impl_->track->performLayout(x, top, std::max(0.0, right - x), height);
        } else {
            impl_->track->setVisible(false);
            impl_->track->performLayout(0, 0, 0, 0);
        }
    }
}

void TreeView::visitChildren(const std::function<void(Node*)>& visitor) {
    if (!impl_) {
        return;
    }
    for (const std::shared_ptr<TreeCell>& row : impl_->rows) {
        if (row) {
            visitor(row.get());
        }
    }
    if (impl_->track) {
        visitor(impl_->track.get());
    }
}

void TreeView::detachChild(Node* child) {
    if (child == nullptr || !impl_) {
        return;
    }
    for (std::shared_ptr<TreeCell>& row : impl_->rows) {
        if (row.get() != child) {
            continue;
        }
        row->detachView();
        if (row->getParent() == this) {
            row->setParent(nullptr);
        }
        row.reset();
        return;
    }
}

double TreeView::preferredContentWidth(double innerAvailable) const {
    if (!impl_) {
        return 0;
    }
    const Font face(computedStyle().fontFamily, computedStyle().fontSize);
    double widest = 0;
    eachVisible([&](TreeItem* item) {
        double width = kDisclosure + kRightPad + static_cast<double>(shownLevel(item)) * impl_->indent;
        if (std::shared_ptr<Node> graphic = item->getGraphic()) {
            width += graphic->measuredWidth(innerAvailable) + kGraphicGap;
        }
        width += face.measureWidth(item->getValue());
        widest = std::max(widest, width);
    });
    return widest;
}

double TreeView::preferredContentHeight(double) const {
    return static_cast<double>(getExpandedItemCount()) * rowSize();
}

void TreeView::renderChildren(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX() + contentLeft());
    const float y = static_cast<float>(getAbsoluteY() + contentTop());
    const float width = static_cast<float>(contentWidth());
    const float height = static_cast<float>(contentHeight());
    renderer.pushClip(x, y, width, height);
    Node::renderChildren(renderer, opacity);
    renderer.popClip();
}

void TreeView::pressScrollBar(const MouseEvent& event) {
    if (!impl_) {
        return;
    }
    const float localX = static_cast<float>(event.x - getAbsoluteX());
    const float localY = static_cast<float>(event.y - getAbsoluteY());
    const ScrollBar::Part where = impl_->vbar.part(localX, localY);
    if (where == ScrollBar::Part::None) {
        impl_->scrollDrag = false;
        return;
    }
    impl_->scrollDrag = true;
    if (where == ScrollBar::Part::Thumb) {
        impl_->scrollGrab = localY - impl_->vbar.thumb;
        return;
    }
    impl_->scroll = impl_->vbar.offsetFromPage(impl_->scroll, where == ScrollBar::Part::After);
    impl_->scrollGrab = impl_->vbar.thumbLength * 0.5f;
}

void TreeView::dragScrollBar(const MouseEvent& event) {
    if (!impl_ || !impl_->scrollDrag) {
        return;
    }
    const float localX = static_cast<float>(event.x - getAbsoluteX());
    const float localY = static_cast<float>(event.y - getAbsoluteY());
    impl_->scroll = impl_->vbar.offsetFromDrag(localX, localY, impl_->scrollGrab);
}

void TreeView::releaseScrollBar() {
    if (impl_) {
        impl_->scrollDrag = false;
    }
}

void TreeScrollBar::renderContent(UiRenderer& renderer, float opacity) {
    if (view_ == nullptr || view_->impl_ == nullptr) {
        return;
    }
    view_->impl_->vbar.draw(renderer, static_cast<float>(view_->getAbsoluteX()),
                            static_cast<float>(view_->getAbsoluteY()), opacity);
}

}  // namespace jadefx
