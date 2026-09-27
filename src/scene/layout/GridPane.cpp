#include "jadefx/scene/layout/GridPane.hpp"

#include "LayoutDetail.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jadefx {
namespace {

constexpr const char* kColumn = "gridpane-column";
constexpr const char* kRow = "gridpane-row";
constexpr const char* kColumnSpan = "gridpane-column-span";
constexpr const char* kRowSpan = "gridpane-row-span";
constexpr const char* kHalignment = "gridpane-halignment";
constexpr const char* kValignment = "gridpane-valignment";
constexpr const char* kHgrow = "gridpane-hgrow";
constexpr const char* kVgrow = "gridpane-vgrow";
constexpr const char* kFillWidth = "gridpane-fill-width";
constexpr const char* kFillHeight = "gridpane-fill-height";
constexpr const char* kMargin = "gridpane-margin";

constexpr double kUnbounded = std::numeric_limits<double>::infinity();

// A stylesheet's row-gap or column-gap, or the pane's own.
double GapOf(float css, double own) { return css >= 0.f ? static_cast<double>(css) : own; }

}  // namespace

// One child and the cells it covers.
struct GridPane::Cell {
    Node* node = nullptr;
    int column = 0;
    int row = 0;
    int columnSpan = 1;
    int rowSpan = 1;
    Insets margin;
};

// One column or row: how big it wants to be, how far it may go, and how it grows.
struct GridPane::Track {
    double pref = 0;
    double min = 0;
    double max = kUnbounded;
    double percent = -1;
    Priority grow = Priority::Never;
};

namespace {

Priority Stronger(Priority a, Priority b) { return static_cast<int>(a) < static_cast<int>(b) ? a : b; }

}  // namespace

// Percentages first, then preferred sizes; then the growers share any extra, and
// every track gives up room toward its minimum when there is too little.
std::vector<double> GridPane::resolveTracks(const std::vector<Track>& tracks, double available, double gap) {
    std::vector<double> sizes(tracks.size());
    if (tracks.empty()) {
        return sizes;
    }
    const double gaps = gap * static_cast<double>(tracks.size() - 1);
    const double room = std::max(0.0, available - gaps);
    double used = 0;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        const Track& track = tracks[i];
        const double wanted = track.percent >= 0 ? room * track.percent / 100.0 : track.pref;
        sizes[i] = std::clamp(wanted, track.min, std::max(track.min, track.max));
        used += sizes[i];
    }
    double extra = room - used;
    if (extra > 0.5) {
        // Always first, then Sometimes. A track at its maximum drops out and leaves the rest to share.
        for (const Priority level : {Priority::Always, Priority::Sometimes}) {
            std::vector<std::size_t> growers;
            for (std::size_t i = 0; i < tracks.size(); ++i) {
                if (tracks[i].grow == level && tracks[i].percent < 0) {
                    growers.push_back(i);
                }
            }
            while (extra > 0.5 && !growers.empty()) {
                const double share = extra / static_cast<double>(growers.size());
                std::vector<std::size_t> open;
                for (const std::size_t i : growers) {
                    const double grown = std::min(sizes[i] + share, tracks[i].max);
                    extra -= grown - sizes[i];
                    sizes[i] = grown;
                    if (grown < tracks[i].max) {
                        open.push_back(i);
                    }
                }
                if (open.size() == growers.size()) {
                    break;
                }
                growers = std::move(open);
            }
            if (extra <= 0.5) {
                break;
            }
        }
    } else if (extra < -0.5) {
        // Too little room: every track that can shrink gives up its share of the excess.
        double give = 0;
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            give += std::max(0.0, sizes[i] - tracks[i].min);
        }
        const double shrink = std::min(-extra, give);
        if (give > 0) {
            for (std::size_t i = 0; i < tracks.size(); ++i) {
                const double spare = std::max(0.0, sizes[i] - tracks[i].min);
                sizes[i] -= shrink * spare / give;
            }
        }
    }
    return sizes;
}

void GridPane::widenForSpan(std::vector<Track>& tracks, int first, int span, double needed, double gap) {
    double have = gap * static_cast<double>(span - 1);
    for (int i = first; i < first + span; ++i) {
        have += tracks[static_cast<std::size_t>(i)].pref;
    }
    if (needed <= have) {
        return;
    }
    const double each = (needed - have) / static_cast<double>(span);
    for (int i = first; i < first + span; ++i) {
        Track& track = tracks[static_cast<std::size_t>(i)];
        track.pref = std::min(track.pref + each, track.max);
    }
}

GridPane::GridPane() { setAlignment(Pos::TopLeft); }

void GridPane::setColumnIndex(Node& child, int index) { layout_detail::SetConstraint(child, kColumn, std::max(0, index)); }
int GridPane::getColumnIndex(const Node& child) { return layout_detail::GetConstraint<int>(child, kColumn).value_or(0); }
void GridPane::setRowIndex(Node& child, int index) { layout_detail::SetConstraint(child, kRow, std::max(0, index)); }
int GridPane::getRowIndex(const Node& child) { return layout_detail::GetConstraint<int>(child, kRow).value_or(0); }
void GridPane::setColumnSpan(Node& child, int span) { layout_detail::SetConstraint(child, kColumnSpan, span); }
int GridPane::getColumnSpan(const Node& child) {
    return layout_detail::GetConstraint<int>(child, kColumnSpan).value_or(1);
}
void GridPane::setRowSpan(Node& child, int span) { layout_detail::SetConstraint(child, kRowSpan, span); }
int GridPane::getRowSpan(const Node& child) { return layout_detail::GetConstraint<int>(child, kRowSpan).value_or(1); }

void GridPane::setConstraints(Node& child, int columnIndex, int rowIndex, int columnSpan, int rowSpan) {
    setColumnIndex(child, columnIndex);
    setRowIndex(child, rowIndex);
    setColumnSpan(child, columnSpan);
    setRowSpan(child, rowSpan);
}

void GridPane::setHalignment(Node& child, HPos alignment) { layout_detail::SetConstraint(child, kHalignment, alignment); }
void GridPane::setValignment(Node& child, VPos alignment) { layout_detail::SetConstraint(child, kValignment, alignment); }
void GridPane::setHgrow(Node& child, Priority priority) { layout_detail::SetConstraint(child, kHgrow, priority); }
void GridPane::setVgrow(Node& child, Priority priority) { layout_detail::SetConstraint(child, kVgrow, priority); }
void GridPane::setFillWidth(Node& child, bool fill) { layout_detail::SetConstraint(child, kFillWidth, fill); }
void GridPane::setFillHeight(Node& child, bool fill) { layout_detail::SetConstraint(child, kFillHeight, fill); }
void GridPane::setMargin(Node& child, const Insets& margin) { layout_detail::SetConstraint(child, kMargin, margin); }
Insets GridPane::getMargin(const Node& child) {
    return layout_detail::GetConstraint<Insets>(child, kMargin).value_or(Insets{});
}

void GridPane::clearConstraints(Node& child) {
    for (const char* key : {kColumn, kRow, kColumnSpan, kRowSpan, kHalignment, kValignment, kHgrow, kVgrow, kFillWidth,
                            kFillHeight, kMargin}) {
        child.getProperties().erase(key);
    }
}

void GridPane::add(std::shared_ptr<Node> child, int columnIndex, int rowIndex, int columnSpan, int rowSpan) {
    if (!child) {
        return;
    }
    setConstraints(*child, columnIndex, rowIndex, columnSpan, rowSpan);
    getChildren().add(std::move(child));
}

int GridPane::nextFree(bool inRow, int index) const {
    int next = 0;
    for (const std::shared_ptr<Node>& child : getChildren()) {
        if (!child) {
            continue;
        }
        const int at = inRow ? getRowIndex(*child) : getColumnIndex(*child);
        if (at != index) {
            continue;
        }
        const int start = inRow ? getColumnIndex(*child) : getRowIndex(*child);
        const int span = inRow ? getColumnSpan(*child) : getRowSpan(*child);
        next = std::max(next, start + std::max(1, span));
    }
    return next;
}

void GridPane::addRow(int rowIndex, std::initializer_list<std::shared_ptr<Node>> children) {
    int column = nextFree(true, rowIndex);
    for (const std::shared_ptr<Node>& child : children) {
        add(child, column++, rowIndex);
    }
}

void GridPane::addColumn(int columnIndex, std::initializer_list<std::shared_ptr<Node>> children) {
    int row = nextFree(false, columnIndex);
    for (const std::shared_ptr<Node>& child : children) {
        add(child, columnIndex, row++);
    }
}

std::vector<GridPane::Cell> GridPane::cells() const {
    std::vector<Cell> out;
    int columns = static_cast<int>(columns_.size());
    int rows = static_cast<int>(rows_.size());
    for (const std::shared_ptr<Node>& child : getChildren()) {
        if (!child || !child->isVisible()) {
            continue;
        }
        Cell cell;
        cell.node = child.get();
        cell.column = getColumnIndex(*child);
        cell.row = getRowIndex(*child);
        cell.columnSpan = getColumnSpan(*child);
        cell.rowSpan = getRowSpan(*child);
        cell.margin = getMargin(*child);
        columns = std::max(columns, cell.column + std::max(1, cell.columnSpan));
        rows = std::max(rows, cell.row + std::max(1, cell.rowSpan));
        out.push_back(cell);
    }
    // REMAINING reaches the last column or row, now that their counts are known.
    for (Cell& cell : out) {
        cell.columnSpan = cell.columnSpan == REMAINING ? columns - cell.column : std::max(1, cell.columnSpan);
        cell.rowSpan = cell.rowSpan == REMAINING ? rows - cell.row : std::max(1, cell.rowSpan);
    }
    return out;
}

std::vector<GridPane::Track> GridPane::columnTracks(const std::vector<Cell>& cells, double available) const {
    int count = static_cast<int>(columns_.size());
    for (const Cell& cell : cells) {
        count = std::max(count, cell.column + cell.columnSpan);
    }
    std::vector<Track> tracks(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < tracks.size() && i < columns_.size(); ++i) {
        const ColumnConstraints& constraint = columns_[i];
        Track& track = tracks[i];
        track.min = std::max(0.0, constraint.minWidth);
        track.max = constraint.maxWidth >= 0 ? constraint.maxWidth : kUnbounded;
        track.percent = constraint.percentWidth;
        track.grow = constraint.hgrow;
    }
    const double gap = GapOf(computedStyle().columnGap, hgap_);
    for (const bool spanning : {false, true}) {
        for (const Cell& cell : cells) {
            if ((cell.columnSpan > 1) != spanning) {
                continue;
            }
            const double wanted = cell.node->measuredWidth(available) + cell.margin.width();
            if (!spanning) {
                Track& track = tracks[static_cast<std::size_t>(cell.column)];
                track.pref = std::max(track.pref, wanted);
                if (const auto grow = layout_detail::GetConstraint<Priority>(*cell.node, kHgrow)) {
                    track.grow = Stronger(track.grow, *grow);
                }
            } else {
                widenForSpan(tracks, cell.column, cell.columnSpan, wanted, gap);
            }
        }
    }
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (i < columns_.size() && columns_[i].prefWidth >= 0) {
            tracks[i].pref = columns_[i].prefWidth;
        }
        tracks[i].pref = std::clamp(tracks[i].pref, tracks[i].min, std::max(tracks[i].min, tracks[i].max));
    }
    return tracks;
}

std::vector<GridPane::Track> GridPane::rowTracks(const std::vector<Cell>& cells, const std::vector<double>& widths,
                                                 double available) const {
    int count = static_cast<int>(rows_.size());
    for (const Cell& cell : cells) {
        count = std::max(count, cell.row + cell.rowSpan);
    }
    std::vector<Track> tracks(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < tracks.size() && i < rows_.size(); ++i) {
        const RowConstraints& constraint = rows_[i];
        Track& track = tracks[i];
        track.min = std::max(0.0, constraint.minHeight);
        track.max = constraint.maxHeight >= 0 ? constraint.maxHeight : kUnbounded;
        track.percent = constraint.percentHeight;
        track.grow = constraint.vgrow;
    }
    const double hgap = GapOf(computedStyle().columnGap, hgap_);
    const double vgap = GapOf(computedStyle().rowGap, vgap_);
    for (const bool spanning : {false, true}) {
        for (const Cell& cell : cells) {
            if ((cell.rowSpan > 1) != spanning) {
                continue;
            }
            // The child is measured at the width its cell will have.
            double cellWidth = hgap * static_cast<double>(cell.columnSpan - 1);
            for (int i = cell.column; i < cell.column + cell.columnSpan && i < static_cast<int>(widths.size()); ++i) {
                cellWidth += widths[static_cast<std::size_t>(i)];
            }
            const double inner = std::max(0.0, cellWidth - cell.margin.width());
            const double width = std::min(cell.node->measuredWidth(inner), inner);
            const double wanted = cell.node->measuredHeight(width, available) + cell.margin.height();
            if (!spanning) {
                Track& track = tracks[static_cast<std::size_t>(cell.row)];
                track.pref = std::max(track.pref, wanted);
                if (const auto grow = layout_detail::GetConstraint<Priority>(*cell.node, kVgrow)) {
                    track.grow = Stronger(track.grow, *grow);
                }
            } else {
                widenForSpan(tracks, cell.row, cell.rowSpan, wanted, vgap);
            }
        }
    }
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (i < rows_.size() && rows_[i].prefHeight >= 0) {
            tracks[i].pref = rows_[i].prefHeight;
        }
        tracks[i].pref = std::clamp(tracks[i].pref, tracks[i].min, std::max(tracks[i].min, tracks[i].max));
    }
    return tracks;
}

int GridPane::getColumnCount() const { return static_cast<int>(columnWidths_.size()); }

int GridPane::getRowCount() const { return static_cast<int>(rowHeights_.size()); }

void GridPane::layoutChildren() {
    const double width = contentWidth();
    const double height = contentHeight();
    const double hgap = GapOf(computedStyle().columnGap, hgap_);
    const double vgap = GapOf(computedStyle().rowGap, vgap_);
    const std::vector<Cell> placed = cells();
    columnWidths_ = resolveTracks(columnTracks(placed, width), width, hgap);
    rowHeights_ = resolveTracks(rowTracks(placed, columnWidths_, height), height, vgap);

    auto total = [](const std::vector<double>& sizes, double gap) {
        double sum = sizes.empty() ? 0.0 : gap * static_cast<double>(sizes.size() - 1);
        for (const double size : sizes) {
            sum += size;
        }
        return sum;
    };
    const Pos align = usingAlignment();
    gridLeft_ = contentLeft() + layout_detail::Align(width, total(columnWidths_, hgap), layout_detail::Horizontal(align));
    gridTop_ = contentTop() + layout_detail::Align(height, total(rowHeights_, vgap), layout_detail::Vertical(align));
    // Where each column and row starts.
    std::vector<double> xs(columnWidths_.size() + 1, gridLeft_);
    for (std::size_t i = 0; i < columnWidths_.size(); ++i) {
        xs[i + 1] = xs[i] + columnWidths_[i] + hgap;
    }
    std::vector<double> ys(rowHeights_.size() + 1, gridTop_);
    for (std::size_t i = 0; i < rowHeights_.size(); ++i) {
        ys[i + 1] = ys[i] + rowHeights_[i] + vgap;
    }

    for (const Cell& cell : placed) {
        const std::size_t column = static_cast<std::size_t>(cell.column);
        const std::size_t row = static_cast<std::size_t>(cell.row);
        const double cellX = xs[column] + cell.margin.left;
        const double cellY = ys[row] + cell.margin.top;
        const double cellW = std::max(0.0, xs[column + static_cast<std::size_t>(cell.columnSpan)] - hgap - xs[column] -
                                                cell.margin.width());
        const double cellH =
            std::max(0.0, ys[row + static_cast<std::size_t>(cell.rowSpan)] - vgap - ys[row] - cell.margin.height());
        const ColumnConstraints* columnRule = column < columns_.size() ? &columns_[column] : nullptr;
        const RowConstraints* rowRule = row < rows_.size() ? &rows_[row] : nullptr;
        const bool fillWidth = layout_detail::GetConstraint<bool>(*cell.node, kFillWidth)
                                   .value_or(columnRule != nullptr && columnRule->fillWidth);
        const bool fillHeight = layout_detail::GetConstraint<bool>(*cell.node, kFillHeight)
                                    .value_or(rowRule != nullptr && rowRule->fillHeight);
        const HPos halign = layout_detail::GetConstraint<HPos>(*cell.node, kHalignment)
                                .value_or(columnRule != nullptr && columnRule->halignment ? *columnRule->halignment
                                                                                          : HPos::Left);
        const VPos valign = layout_detail::GetConstraint<VPos>(*cell.node, kValignment)
                                .value_or(rowRule != nullptr && rowRule->valignment ? *rowRule->valignment
                                                                                    : VPos::Center);
        const double childW = fillWidth ? cellW : std::min(cell.node->measuredWidth(cellW), cellW);
        const double childH = fillHeight ? cellH : std::min(cell.node->measuredHeight(childW, cellH), cellH);
        const int horizontal = halign == HPos::Center ? 1 : (halign == HPos::Right ? 2 : 0);
        const int vertical = valign == VPos::Center ? 1 : (valign == VPos::Bottom ? 2 : 0);
        cell.node->performLayout(cellX + layout_detail::Align(cellW, childW, horizontal),
                                 cellY + layout_detail::Align(cellH, childH, vertical), childW, childH);
    }
}

double GridPane::preferredContentWidth(double innerAvailable) const {
    const std::vector<Track> tracks = columnTracks(cells(), innerAvailable);
    double width = tracks.empty() ? 0.0 : GapOf(computedStyle().columnGap, hgap_) * static_cast<double>(tracks.size() - 1);
    for (const Track& track : tracks) {
        width += track.pref;
    }
    return width;
}

double GridPane::preferredContentHeight(double innerWidth) const {
    const std::vector<Cell> placed = cells();
    const std::vector<double> widths =
        resolveTracks(columnTracks(placed, innerWidth), innerWidth, GapOf(computedStyle().columnGap, hgap_));
    const std::vector<Track> tracks = rowTracks(placed, widths, -1);
    double height = tracks.empty() ? 0.0 : GapOf(computedStyle().rowGap, vgap_) * static_cast<double>(tracks.size() - 1);
    for (const Track& track : tracks) {
        height += track.pref;
    }
    return height;
}

void GridPane::renderContent(UiRenderer& renderer, float opacity) {
    if (!gridLines_ || columnWidths_.empty() || rowHeights_.empty()) {
        return;
    }
    const float x0 = static_cast<float>(getAbsoluteX() + gridLeft_);
    const float y0 = static_cast<float>(getAbsoluteY() + gridTop_);
    const double hgap = GapOf(computedStyle().columnGap, hgap_);
    const double vgap = GapOf(computedStyle().rowGap, vgap_);
    Color line = themeColor(ThemeColor::Accent);
    line.a *= 0.6f * opacity;
    const float radius[4] = {};
    const float sides[4] = {1.f, 1.f, 1.f, 1.f};
    float y = y0;
    for (const double rowHeight : rowHeights_) {
        float x = x0;
        for (const double columnWidth : columnWidths_) {
            renderer.strokeRounded(x, y, static_cast<float>(columnWidth), static_cast<float>(rowHeight), radius, sides,
                                   line);
            x += static_cast<float>(columnWidth + hgap);
        }
        y += static_cast<float>(rowHeight + vgap);
    }
}

}  // namespace jadefx
