#pragma once

#include "jadefx/scene/layout/Pane.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace jadefx {

// Sizes for one column of a GridPane, as JavaFX's ColumnConstraints. A negative
// width is unset: min and max then come from nothing, and pref from the widest
// child. percentWidth takes that share of the grid's width after the gaps.
// hgrow, halignment, and fillWidth apply to every child in the column that
// does not set its own.
struct ColumnConstraints {
    double minWidth = -1;
    double prefWidth = -1;
    double maxWidth = -1;
    double percentWidth = -1;
    Priority hgrow = Priority::Never;
    std::optional<HPos> halignment;
    bool fillWidth = false;

    ColumnConstraints() = default;
    // A fixed width.
    explicit ColumnConstraints(double width) : minWidth(width), prefWidth(width), maxWidth(width) {}
    ColumnConstraints(double min, double pref, double max) : minWidth(min), prefWidth(pref), maxWidth(max) {}
};

// Sizes for one row of a GridPane, as JavaFX's RowConstraints. See ColumnConstraints.
struct RowConstraints {
    double minHeight = -1;
    double prefHeight = -1;
    double maxHeight = -1;
    double percentHeight = -1;
    Priority vgrow = Priority::Never;
    std::optional<VPos> valignment;
    bool fillHeight = false;

    RowConstraints() = default;
    explicit RowConstraints(double height) : minHeight(height), prefHeight(height), maxHeight(height) {}
    RowConstraints(double min, double pref, double max) : minHeight(min), prefHeight(pref), maxHeight(max) {}
};

// Children in rows and columns, in the shape of OpenJFX GridPane.
// A child sits at its column and row index, spanning columnSpan columns and
// rowSpan rows; REMAINING spans to the last one. A column is as wide as its
// widest child, or as its constraints say, and the same for a row's height.
// When the pane is wider than the columns want, columns with hgrow Always share
// the extra, or with none of those, columns with Sometimes; rows likewise with
// vgrow. When it is narrower, columns shrink toward their minimums.
// A child keeps its preferred size inside its cell, placed by its halignment
// and valignment, unless fillWidth or fillHeight stretches it. The grid as a
// whole sits in the pane by setAlignment (top left by default).
// Constraints are set on the child, before or after it is added, with the
// static setters, as in JavaFX.
class GridPane : public Pane {
public:
    // A span that reaches the last column or row.
    static constexpr int REMAINING = -1;

    GridPane();

    const char* getElementType() const override { return "gridpane"; }

    // Adds child at a column and row, spanning as many as given.
    void add(std::shared_ptr<Node> child, int columnIndex, int rowIndex, int columnSpan = 1, int rowSpan = 1);
    // Adds children across one row, starting at column 0 past the last child already in it.
    void addRow(int rowIndex, std::initializer_list<std::shared_ptr<Node>> children);
    // Adds children down one column, starting past the last child already in it.
    void addColumn(int columnIndex, std::initializer_list<std::shared_ptr<Node>> children);

    void setHgap(double gap) { setLayoutValue(hgap_, gap); }
    double getHgap() const { return hgap_; }
    void setVgap(double gap) { setLayoutValue(vgap_, gap); }
    double getVgap() const { return vgap_; }
    // Draws each cell's edges, to see how the grid divides the space. Only renderContent
    // reads this, so it does not touch layout.
    void setGridLinesVisible(bool visible) { gridLines_ = visible; }
    bool isGridLinesVisible() const { return gridLines_; }

    // Mutable, since a caller edits entries or reassigns the whole list after getting it
    // (ColumnConstraints and RowConstraints are not Nodes and cannot mark themselves), so
    // getting either one marks the grid for layout.
    std::vector<ColumnConstraints>& getColumnConstraints() {
        markLayoutDirty();
        return columns_;
    }
    std::vector<RowConstraints>& getRowConstraints() {
        markLayoutDirty();
        return rows_;
    }

    // Columns and rows as of the last layout, and their sizes.
    int getColumnCount() const;
    int getRowCount() const;
    const std::vector<double>& getColumnWidths() const { return columnWidths_; }
    const std::vector<double>& getRowHeights() const { return rowHeights_; }

    // A child's cell. Unset indexes are 0 and unset spans are 1.
    static void setColumnIndex(Node& child, int index);
    static int getColumnIndex(const Node& child);
    static void setRowIndex(Node& child, int index);
    static int getRowIndex(const Node& child);
    static void setColumnSpan(Node& child, int span);
    static int getColumnSpan(const Node& child);
    static void setRowSpan(Node& child, int span);
    static int getRowSpan(const Node& child);
    static void setConstraints(Node& child, int columnIndex, int rowIndex, int columnSpan = 1, int rowSpan = 1);
    // Per child, over the column's and row's constraints.
    static void setHalignment(Node& child, HPos alignment);
    static void setValignment(Node& child, VPos alignment);
    static void setHgrow(Node& child, Priority priority);
    static void setVgrow(Node& child, Priority priority);
    static void setFillWidth(Node& child, bool fill);
    static void setFillHeight(Node& child, bool fill);
    // Space kept around the child inside its cell.
    static void setMargin(Node& child, const Insets& margin);
    static Insets getMargin(const Node& child);
    // Forgets every GridPane constraint on the child.
    static void clearConstraints(Node& child);

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    struct Cell;
    struct Track;

    std::vector<Cell> cells() const;
    // Preferred, minimum, and maximum size of each column or row, and how it grows.
    std::vector<Track> columnTracks(const std::vector<Cell>& cells, double available) const;
    std::vector<Track> rowTracks(const std::vector<Cell>& cells, const std::vector<double>& widths,
                                 double available) const;
    int nextFree(bool inRow, int index) const;
    // Sizes tracks to fill available points, gaps included.
    static std::vector<double> resolveTracks(const std::vector<Track>& tracks, double available, double gap);
    // Spreads what a spanning child needs beyond its tracks' preferred sizes across them.
    static void widenForSpan(std::vector<Track>& tracks, int first, int span, double needed, double gap);

    double hgap_ = 0;
    double vgap_ = 0;
    bool gridLines_ = false;
    std::vector<ColumnConstraints> columns_;
    std::vector<RowConstraints> rows_;
    std::vector<double> columnWidths_;
    std::vector<double> rowHeights_;
    double gridLeft_ = 0;
    double gridTop_ = 0;
};

}  // namespace jadefx
