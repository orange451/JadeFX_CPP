#include "jadefx/scene/controls/TableView.hpp"

#include "jadefx/scene/controls/Label.hpp"
#include "ScrollSupport.hpp"
#include "VirtualFlow.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace jadefx {

namespace {

// Points either side of a header's right edge that grab it for resizing.
constexpr double kResizeGrip = 4;
constexpr double kMinHeaderHeight = 24;
// Room a header keeps on its right for the sort arrow.
constexpr double kArrowRoom = 16;
// JavaFX's TableView asks for this much height unless told otherwise.
constexpr double kPrefHeight = 400;

// One row of a table: a cell for each visible column, side by side.
class TableRow : public IndexedCell {
public:
    explicit TableRow(TableViewBase& table) : table_(&table) {
        getClassList().add("table-row-cell");
        setPadding(Insets::empty());
    }

    const char* getElementType() const override { return "tr"; }

    // Makes the row's cells match the visible columns, keeping cells whose
    // column and cell factory are unchanged.
    void syncColumns(const std::vector<TableColumnBase*>& columns, int version) {
        if (version == version_) {
            return;
        }
        version_ = version;
        std::vector<Slot> next;
        next.reserve(columns.size());
        for (TableColumnBase* column : columns) {
            auto kept = std::find_if(slots_.begin(), slots_.end(), [column](const Slot& slot) {
                return slot.column == column && slot.generation == column->getCellGeneration();
            });
            if (kept != slots_.end()) {
                next.push_back(std::move(*kept));
                kept->column = nullptr;
                continue;
            }
            Slot slot{column, column->getCellGeneration(), column->createCell()};
            if (slot.cell) {
                slot.cell->setFocusTraversable(false);
                for (const std::string& name : column->getStyleClass()) {
                    slot.cell->getClassList().add(name);
                }
                children().add(slot.cell);
            }
            next.push_back(std::move(slot));
        }
        for (Slot& slot : slots_) {
            if (slot.column != nullptr && slot.cell) {
                Node* old = slot.cell.get();
                children().removeIf([old](const std::shared_ptr<Node>& child) { return child.get() == old; });
            }
        }
        slots_ = std::move(next);
        // The cells are placed in column order, and the widths add up to the row's.
        markLayoutDirty();
    }

    void updateIndex(int index) override {
        IndexedCell::updateIndex(index);
        updateEmpty(index < 0);
        for (Slot& slot : slots_) {
            if (slot.cell) {
                slot.column->bindCell(*slot.cell, index);
            }
        }
    }

    IndexedCell* cellFor(const TableColumnBase* column) const {
        for (const Slot& slot : slots_) {
            if (slot.column == column) {
                return slot.cell.get();
            }
        }
        return nullptr;
    }

    template <typename Visit>
    void forEachCell(Visit visit) const {
        for (const Slot& slot : slots_) {
            if (slot.cell) {
                visit(*slot.column, *slot.cell);
            }
        }
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (table_ != nullptr && !isEmpty()) {
            table_->rowPressed(getIndex(), event);
        }
    }

protected:
    void layoutChildren() override {
        double x = contentLeft();
        for (const Slot& slot : slots_) {
            const double width = slot.column->getWidth();
            if (slot.cell) {
                slot.cell->performLayout(x, contentTop(), width, contentHeight());
            }
            x += width;
        }
    }

    double preferredContentWidth(double) const override {
        double width = 0;
        for (const Slot& slot : slots_) {
            width += slot.column->getWidth();
        }
        return width;
    }

    double preferredContentHeight(double) const override {
        double height = 0;
        for (const Slot& slot : slots_) {
            if (slot.cell) {
                height = std::max(height, slot.cell->measuredHeight(slot.column->getWidth(), -1));
            }
        }
        return height;
    }

private:
    struct Slot {
        TableColumnBase* column = nullptr;
        int generation = 0;
        std::shared_ptr<IndexedCell> cell;
    };

    TableViewBase* table_ = nullptr;
    std::vector<Slot> slots_;
    int version_ = -1;
};

// The header row: the column headers, clipped, moving sideways with the rows.
class TableHeaderRow : public scroll::ClipRegion {
public:
    // Its background, like each header's, comes from the user-agent stylesheet
    // and fills past the last column, over the scroll bar.
    TableHeaderRow() : ClipRegion("column-header-background") { setBorder(Insets{0, 0, 1, 0}); }

    const char* getElementType() const override { return "thead"; }
};

// Where a dragged column would land.
class TableDropMarker : public Region {
public:
    TableDropMarker() {
        getClassList().add("column-drag-marker");
        setMouseTransparent(true);
    }

    const char* getElementType() const override { return "column-drag-marker"; }
};

}  // namespace

// One column's header: its text and graphic, and a sort arrow.
class TableColumnHeader : public Labeled {
public:
    TableColumnHeader(TableViewBase& table, TableColumnBase& column) : Labeled(""), table_(&table), column_(&column) {
        getClassList().add("column-header");
        for (const std::string& name : column.getStyleClass()) {
            getClassList().add(name);
        }
        setAlignment(Pos::CenterLeft);
        setPadding(Insets{IndexedCell::kPaddingY, IndexedCell::kPaddingX + kArrowRoom, IndexedCell::kPaddingY,
                          IndexedCell::kPaddingX});
        setBorder(Insets{0, 1, 1, 0});
        setFocusTraversable(false);
        refresh();
    }

    const char* getElementType() const override { return "th"; }

    TableColumnBase* column() const { return column_; }

    // Takes the column's text, graphic, id, and sort state.
    void refresh() {
        setText(column_->getText());
        if (getGraphic() != column_->getGraphic()) {
            setGraphic(column_->getGraphic());
        }
        setElementId(column_->getId());
        const std::vector<TableColumnBase*>& order = table_->getSortOrder();
        const auto found = std::find(order.begin(), order.end(), column_);
        sortRank_ = found == order.end() ? 0 : static_cast<int>(found - order.begin()) + 1;
        sortCount_ = static_cast<int>(order.size());
        setPseudoState("sorted", sortRank_ > 0);
        setPseudoState("ascending", sortRank_ > 0 && column_->getSortType() == SortType::Ascending);
        setPseudoState("descending", sortRank_ > 0 && column_->getSortType() == SortType::Descending);
    }

    bool onResizeGrip(double x) const {
        return column_->isResizable() && x >= getAbsoluteX() + getWidth() - kResizeGrip;
    }

    Cursor cursorAt(double x, double y) const override {
        return onResizeGrip(x) ? Cursor::EwResize : Labeled::cursorAt(x, y);
    }

    void handleMousePressed(const MouseEvent& event) override {
        table_->headerPressed(*column_, onResizeGrip(event.x), event);
    }
    void handleMouseDragged(const MouseEvent& event) override { table_->headerDragged(event); }
    void handleMouseReleased(const MouseEvent& event) override { table_->headerReleased(event); }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        Labeled::renderContent(renderer, opacity);
        if (sortRank_ == 0) {
            return;
        }
        // A small triangle on the right: up for ascending, down for descending.
        Color color = computedStyle().color;
        color.a *= 0.7f * opacity;
        const float size = 4.f;
        const float cx = static_cast<float>(getAbsoluteX() + getWidth() - 8 - kArrowRoom * 0.5);
        const float cy = static_cast<float>(getAbsoluteY() + getHeight() * 0.5);
        const bool up = column_->getSortType() == SortType::Ascending;
        const float radius[4] = {};
        const float at = 0.f;
        // Rows of one-point bars make the triangle, widest at its base.
        for (int i = 0; i < static_cast<int>(size); ++i) {
            const float half = static_cast<float>(i) + 0.5f;
            const float y = up ? cy - size * 0.5f + static_cast<float>(i) : cy + size * 0.5f - static_cast<float>(i) - 1.f;
            renderer.fillRounded(cx - half, y, half * 2.f, 1.f, radius, &color, &at, 1, 0.f);
        }
        if (sortCount_ > 1) {
            const std::string rank = std::to_string(sortRank_);
            renderer.text(cx + size + 1.f, cy - 6.f, rank, computedStyle().fontFamily, 10.f, color,
                          computedStyle().subpixel);
        }
    }

private:
    TableViewBase* table_;
    TableColumnBase* column_;
    int sortRank_ = 0;
    int sortCount_ = 0;
};

struct TableViewBase::Header {
    std::shared_ptr<TableHeaderRow> row = std::make_shared<TableHeaderRow>();
    std::shared_ptr<TableDropMarker> marker = std::make_shared<TableDropMarker>();
    std::vector<std::shared_ptr<TableColumnHeader>> cells;
    // The visible columns and their cell factories, as the rows last saw them.
    std::vector<std::pair<TableColumnBase*, int>> signature;
    int version = 0;
    // A header gesture in progress.
    enum class Gesture { None, Press, Resize, Move } gesture = Gesture::None;
    TableColumnBase* column = nullptr;
    double startX = 0;
    double startWidth = 0;
    TableColumnBase* neighbour = nullptr;
    double neighbourWidth = 0;
    int drop = -1;
    // True while a dragged column is taken out of the list and put back.
    bool moving = false;
};

TableViewBase::TableViewBase(std::unique_ptr<MultipleSelectionModel> selection)
    : RowViewBase(std::move(selection), Orientation::Vertical), header_(std::make_unique<Header>()) {
    getClassList().add("table-view");
    header_->marker->setVisible(false);
    header_->row->items().add(header_->marker);
    children().add(header_->row);
    setPlaceholder(std::make_shared<Label>("No content in table"));
    columns_.setAddCallback([this](const std::shared_ptr<TableColumnBase>& column) { columnAdded(column); });
    columns_.setRemoveCallback([this](const std::shared_ptr<TableColumnBase>& column) { columnRemoved(column); });
}

TableViewBase::~TableViewBase() {
    columns_.setAddCallback(nullptr);
    columns_.setRemoveCallback(nullptr);
    for (const std::shared_ptr<TableColumnBase>& column : columns_) {
        if (column && column->table_ == this) {
            column->table_ = nullptr;
            column->changed_ = nullptr;
        }
    }
}

void TableViewBase::columnAdded(const std::shared_ptr<TableColumnBase>& column) {
    if (column) {
        column->table_ = this;
        column->changed_ = [this] { columnsChanged(); };
    }
    columnsChanged();
}

void TableViewBase::columnsChanged() {
    // getVisibleLeafColumns, the header, and the rows all read the column list. The
    // rows sit in the flow, which keeps its own flags, so it binds them again.
    markLayoutDirty();
    markStyleDirty(StyleDirt::Subtree);
    flow().refresh();
}

void TableViewBase::columnRemoved(const std::shared_ptr<TableColumnBase>& column) {
    if (!column) {
        return;
    }
    // A column being dragged to another place leaves the list only for a moment.
    if (header_->moving) {
        return;
    }
    column->table_ = nullptr;
    column->changed_ = nullptr;
    TableColumnBase* raw = column.get();
    if (editColumn_ == raw) {
        edit(-1, nullptr);
    }
    if (focusedColumn_ == raw) {
        focusedColumn_ = nullptr;
    }
    sortOrder_.erase(std::remove(sortOrder_.begin(), sortOrder_.end(), raw), sortOrder_.end());
    columnsChanged();
}

std::vector<TableColumnBase*> TableViewBase::getVisibleLeafColumns() const {
    std::vector<TableColumnBase*> visible;
    for (const std::shared_ptr<TableColumnBase>& column : columns_) {
        if (column && column->isVisible()) {
            visible.push_back(column.get());
        }
    }
    return visible;
}

void TableViewBase::setSortOrder(std::vector<TableColumnBase*> order) {
    order.erase(std::remove_if(order.begin(), order.end(),
                               [&](TableColumnBase* column) {
                                   return column == nullptr || column->getTableViewBase() != this;
                               }),
                order.end());
    // Each header's sorted/ascending/descending pseudo-states read the order
    // through its own refresh, called from this table's layoutChildren.
    if (order != sortOrder_) {
        sortOrder_ = std::move(order);
        markLayoutDirty();
        markStyleDirty(StyleDirt::Subtree);
    }
    sort();
}

void TableViewBase::sort() {
    cancelEditing();
    applySort(sortOrder_);
    if (onSort_) {
        onSort_();
    }
}

void TableViewBase::remapRows(const std::vector<int>& where) {
    auto moved = [&where](int index) {
        return index >= 0 && index < static_cast<int>(where.size()) ? where[static_cast<std::size_t>(index)] : index;
    };
    MultipleSelectionModel& selection = getBaseSelectionModel();
    std::vector<int> selected = selection.getSelectedIndices();
    const int anchor = moved(selection.getAnchor());
    const int focused = moved(getFocusModel().getFocusedIndex());
    for (int& index : selected) {
        index = moved(index);
    }
    selection.clearSelection();
    selection.selectIndices(selected);
    selection.setAnchor(anchor);
    getFocusModel().focus(focused);
}

void TableViewBase::toggleSort(TableColumnBase& column, bool add) {
    std::vector<TableColumnBase*> order = sortOrder_;
    const auto found = std::find(order.begin(), order.end(), &column);
    const bool alone = order.size() == 1 && found != order.end();
    if (add || alone) {
        if (found == order.end()) {
            column.setSortType(SortType::Ascending);
            order.push_back(&column);
        } else if (column.getSortType() == SortType::Ascending) {
            column.setSortType(SortType::Descending);
        } else {
            order.erase(found);
        }
    } else {
        column.setSortType(SortType::Ascending);
        order = {&column};
    }
    setSortOrder(std::move(order));
}

void TableViewBase::edit(int row, TableColumnBase* column) {
    const bool allowed = row >= 0 && row < itemCount() && column != nullptr && isEditable() && column->isEditable() &&
                         column->isVisible() && column->getTableViewBase() == this;
    if (!allowed) {
        row = -1;
        column = nullptr;
    }
    if (row == editRow_ && column == editColumn_) {
        return;
    }
    const int previousRow = editRow_;
    TableColumnBase* const previousColumn = editColumn_;
    editRow_ = row;
    editColumn_ = column;
    if (auto* old = dynamic_cast<TableRow*>(flow().getVisibleCell(previousRow))) {
        if (IndexedCell* cell = old->cellFor(previousColumn); cell != nullptr && cell->isEditing()) {
            cell->cancelEdit();
        }
    }
    if (row < 0) {
        return;
    }
    focusedColumn_ = column;
    // The cell starts editing when its row is bound, if it is not on screen yet.
    flow().show(row);
    showColumn(*column);
    if (auto* target = dynamic_cast<TableRow*>(flow().getVisibleCell(row))) {
        if (IndexedCell* cell = target->cellFor(column); cell != nullptr && !cell->isEditing()) {
            cell->startEdit();
        }
    }
}

void TableViewBase::editFocused() {
    TableColumnBase* column = focusedColumn_;
    if (column == nullptr || !column->isEditable()) {
        column = nullptr;
        for (TableColumnBase* candidate : getVisibleLeafColumns()) {
            if (candidate->isEditable()) {
                column = candidate;
                break;
            }
        }
    }
    edit(getFocusModel().getFocusedIndex(), column);
}

void TableViewBase::cellPressed(int row, TableColumnBase* column, const MouseEvent& event) {
    if (event.button == 0) {
        focusedColumn_ = column;
    }
    rowPressed(row, event);
}

bool TableViewBase::handleCrossKey(int key) {
    const std::vector<TableColumnBase*> visible = getVisibleLeafColumns();
    if (visible.empty() || (key != Key::Left && key != Key::Right)) {
        return false;
    }
    auto at = std::find(visible.begin(), visible.end(), focusedColumn_);
    int index = at == visible.end() ? 0 : static_cast<int>(at - visible.begin());
    if (at != visible.end()) {
        index += key == Key::Right ? 1 : -1;
    }
    index = std::clamp(index, 0, static_cast<int>(visible.size()) - 1);
    focusedColumn_ = visible[static_cast<std::size_t>(index)];
    showColumn(*focusedColumn_);
    return true;
}

void TableViewBase::showColumn(const TableColumnBase& column) {
    double start = 0;
    for (const TableColumnBase* each : getVisibleLeafColumns()) {
        if (each == &column) {
            flow().showBreadth(start, start + column.getWidth());
            return;
        }
        start += each->getWidth();
    }
}

std::shared_ptr<IndexedCell> TableViewBase::createRow() { return std::make_shared<TableRow>(*this); }

void TableViewBase::bindRow(IndexedCell& row, int index) {
    auto& tableRow = static_cast<TableRow&>(row);
    tableRow.syncColumns(getVisibleLeafColumns(), header_->version);
    tableRow.updateIndex(index);
}

void TableViewBase::syncEditing(IndexedCell& row) {
    auto& tableRow = static_cast<TableRow&>(row);
    const int index = tableRow.getIndex();
    tableRow.forEachCell([&](TableColumnBase& column, IndexedCell& cell) {
        const bool target = index >= 0 && index == editRow_ && &column == editColumn_;
        if (target && !cell.isEditing()) {
            cell.startEdit();
        } else if (!target && cell.isEditing()) {
            cell.cancelEdit();
        }
    });
}

void TableViewBase::checkColumns(const std::vector<TableColumnBase*>& visible) {
    std::vector<std::pair<TableColumnBase*, int>> signature;
    signature.reserve(visible.size());
    for (TableColumnBase* column : visible) {
        signature.emplace_back(column, column->getCellGeneration());
    }
    if (signature == header_->signature) {
        return;
    }
    header_->signature = std::move(signature);
    ++header_->version;
    refresh();
    // Headers follow the visible columns, keeping the ones that stay.
    std::vector<std::shared_ptr<TableColumnHeader>> cells;
    for (TableColumnBase* column : visible) {
        auto kept = std::find_if(header_->cells.begin(), header_->cells.end(),
                                 [column](const auto& cell) { return cell && cell->column() == column; });
        if (kept != header_->cells.end()) {
            cells.push_back(std::move(*kept));
            continue;
        }
        cells.push_back(std::make_shared<TableColumnHeader>(*this, *column));
        header_->row->items().insert(header_->row->items().size() - 1, cells.back());
    }
    for (const std::shared_ptr<TableColumnHeader>& old : header_->cells) {
        if (old) {
            Node* raw = old.get();
            header_->row->items().removeIf([raw](const std::shared_ptr<Node>& child) { return child.get() == raw; });
        }
    }
    header_->cells = std::move(cells);
}

void TableViewBase::fitColumns(const std::vector<TableColumnBase*>& visible, double available) {
    for (TableColumnBase* column : visible) {
        column->width_ = std::clamp(column->getPrefWidth(), column->getMinWidth(), column->getMaxWidth());
    }
    if (resizePolicy_ != ColumnResizePolicy::Constrained || visible.empty()) {
        return;
    }
    // Scale the columns that can still move until the widths fill the table.
    // A column that hits its minimum or maximum keeps it and leaves the rest to share.
    std::vector<TableColumnBase*> free(visible);
    double fixed = 0;
    for (int round = 0; round < static_cast<int>(visible.size()) && !free.empty(); ++round) {
        double preferred = 0;
        for (TableColumnBase* column : free) {
            preferred += std::max(1.0, column->getPrefWidth());
        }
        const double share = std::max(0.0, available - fixed);
        bool clamped = false;
        for (auto it = free.begin(); it != free.end();) {
            TableColumnBase* column = *it;
            const double wanted = share * std::max(1.0, column->getPrefWidth()) / preferred;
            const double width = std::clamp(wanted, column->getMinWidth(), column->getMaxWidth());
            column->width_ = width;
            if (width != wanted) {
                fixed += width;
                it = free.erase(it);
                clamped = true;
            } else {
                ++it;
            }
        }
        if (!clamped) {
            return;
        }
    }
}

void TableViewBase::layoutHeader(const std::vector<TableColumnBase*>& visible, double left, double top,
                                 double width, double height) {
    header_->row->performLayout(left, top, width, height);
    double x = -std::round(flow().getBreadthOffset());
    for (std::size_t i = 0; i < visible.size() && i < header_->cells.size(); ++i) {
        TableColumnHeader& cell = *header_->cells[i];
        cell.refresh();
        cell.performLayout(x, 0, visible[i]->getWidth(), height);
        x += visible[i]->getWidth();
    }
    TableDropMarker& marker = *header_->marker;
    const bool moving = header_->gesture == Header::Gesture::Move && header_->drop >= 0;
    marker.setVisible(moving);
    if (!moving) {
        marker.performLayout(0, 0, 0, 0);
        return;
    }
    double markerX = -std::round(flow().getBreadthOffset());
    for (int i = 0; i < header_->drop && i < static_cast<int>(visible.size()); ++i) {
        markerX += visible[static_cast<std::size_t>(i)]->getWidth();
    }
    marker.performLayout(markerX - 1, 0, 2, height);
}

void TableViewBase::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const double width = contentWidth();
    const double height = contentHeight();
    const std::vector<TableColumnBase*> visible = getVisibleLeafColumns();
    checkColumns(visible);

    // The rows' vertical bar takes its thickness from the width the columns can fill.
    double headerHeight = kMinHeaderHeight;
    for (const std::shared_ptr<TableColumnHeader>& cell : header_->cells) {
        headerHeight = std::max(headerHeight, cell->measuredHeight(cell->getWidth(), -1));
    }
    const double rowsHeight = std::max(0.0, height - headerHeight);
    const bool vbar = static_cast<double>(itemCount()) * flow().getCellLength() > rowsHeight + 0.5;
    fitColumns(visible, std::max(0.0, width - (vbar ? ScrollBar::kThickness : 0.0)));

    layoutRows(left, top + headerHeight, width, rowsHeight);
    layoutHeader(visible, left, top, width, headerHeight);
}

double TableViewBase::preferredContentWidth(double) const {
    double width = 0;
    for (const TableColumnBase* column : getVisibleLeafColumns()) {
        width += std::clamp(column->getPrefWidth(), column->getMinWidth(), column->getMaxWidth());
    }
    return width + ScrollBar::kThickness;
}

double TableViewBase::preferredContentHeight(double) const { return kPrefHeight; }

void TableViewBase::headerPressed(TableColumnBase& column, bool resize, const MouseEvent& event) {
    if (event.button != 0) {
        return;
    }
    requestFocus();
    Header& header = *header_;
    header.column = &column;
    header.startX = event.x;
    header.startWidth = column.getWidth();
    header.neighbour = nullptr;
    header.drop = -1;
    header.gesture = resize ? Header::Gesture::Resize : Header::Gesture::Press;
    if (!resize || resizePolicy_ != ColumnResizePolicy::Constrained) {
        return;
    }
    // A constrained table trades width with the next column, so the total stays.
    const std::vector<TableColumnBase*> visible = getVisibleLeafColumns();
    const auto at = std::find(visible.begin(), visible.end(), &column);
    if (at == visible.end() || at + 1 == visible.end()) {
        header.gesture = Header::Gesture::None;
        return;
    }
    for (TableColumnBase* each : visible) {
        each->setPrefWidth(each->getWidth());
    }
    header.neighbour = *(at + 1);
    header.neighbourWidth = header.neighbour->getWidth();
}

void TableViewBase::headerDragged(const MouseEvent& event) {
    Header& header = *header_;
    if (header.column == nullptr) {
        return;
    }
    TableColumnBase& column = *header.column;
    const double delta = event.x - header.startX;
    switch (header.gesture) {
        case Header::Gesture::Resize: {
            double width = std::clamp(header.startWidth + delta, column.getMinWidth(), column.getMaxWidth());
            if (header.neighbour != nullptr) {
                TableColumnBase& next = *header.neighbour;
                const double total = header.startWidth + header.neighbourWidth;
                width = std::clamp(width, total - next.getMaxWidth(), total - next.getMinWidth());
                next.setPrefWidth(total - width);
            }
            column.setPrefWidth(width);
            break;
        }
        case Header::Gesture::Press:
            if (!event.stillSincePress && column.isReorderable()) {
                header.gesture = Header::Gesture::Move;
                header.drop = dropIndex(event.x);
            }
            break;
        case Header::Gesture::Move:
            header.drop = dropIndex(event.x);
            break;
        case Header::Gesture::None:
            break;
    }
}

void TableViewBase::headerReleased(const MouseEvent& event) {
    Header& header = *header_;
    TableColumnBase* column = header.column;
    const Header::Gesture gesture = header.gesture;
    const int drop = header.drop;
    header.gesture = Header::Gesture::None;
    header.column = nullptr;
    header.neighbour = nullptr;
    header.drop = -1;
    if (column == nullptr) {
        return;
    }
    if (gesture == Header::Gesture::Press && event.stillSincePress && column->isSortable()) {
        toggleSort(*column, event.shift());
        return;
    }
    if (gesture != Header::Gesture::Move || drop < 0) {
        return;
    }
    // Move the column in the full list to just before the visible column at drop.
    const std::vector<TableColumnBase*> visible = getVisibleLeafColumns();
    TableColumnBase* before = drop < static_cast<int>(visible.size()) ? visible[static_cast<std::size_t>(drop)] : nullptr;
    if (before == column) {
        return;
    }
    std::shared_ptr<TableColumnBase> held;
    header.moving = true;
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].get() == column) {
            held = columns_[i];
            columns_.removeAt(i);
            break;
        }
    }
    std::size_t target = columns_.size();
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].get() == before) {
            target = i;
            break;
        }
    }
    columns_.insert(target, std::move(held));
    header.moving = false;
}

int TableViewBase::dropIndex(double x) const {
    const std::vector<TableColumnBase*> visible = getVisibleLeafColumns();
    double edge = header_->row->getAbsoluteX() - std::round(flow().getBreadthOffset());
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const double width = visible[i]->getWidth();
        if (x < edge + width * 0.5) {
            return static_cast<int>(i);
        }
        edge += width;
    }
    return static_cast<int>(visible.size());
}

}  // namespace jadefx
