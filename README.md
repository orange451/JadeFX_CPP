# JadeFX C++

A cross-platform UI library built on OpenGL. The scene graph, layout panes, and CSS styling follow [JadeFX](https://github.com/orange451/JadeFX): a window has a scene, the scene has a tree of nodes, and stylesheets describe how those nodes look.

Desktop windows use GLFW and OpenGL 3.3 (4.1 on macOS). iOS, Android, and Emscripten use [GLFM](https://github.com/brackeen/glfm) and OpenGL ES 3. The same scene graph runs on both.

## Build

From the project directory on macOS or Linux:

```sh
make
make test
make run
```

`make run` opens the Purple sample: a phone-sized window with a gradient, a label, and two buttons. `make test` checks color parsing, text measurement, box layout, and click handling without keeping a window open.

Windows, from a developer prompt:

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\jadefx-tests.exe
build\Release\jadefx-purple.exe
build\Release\jadefx-border.exe
build\Release\jadefx-tabs.exe
build\Release\jadefx-tree.exe
build\Release\jadefx-lists.exe
build\Release\jadefx-split.exe
```

A system GLFW is used when CMake can find it. Otherwise CMake downloads GLFW 3.5.1. Linux needs the X11 and Wayland development packages to build that copy. CMake also downloads the stb_truetype header used to read fonts.

There is also a smaller window, a BorderPane sample (`make border`), a TabPane sample (`make tabs`), a TreeView sample (`make tree`), a ListView sample (`make lists`), and a SplitPane sample (`make split`):

```sh
./build/JadeFX\ Hello.app/Contents/MacOS/JadeFX\ Hello   # macOS
./build/jadefx-hello                                     # Linux
./build/JadeFX\ Border.app/Contents/MacOS/JadeFX\ Border # macOS
./build/jadefx-border                                    # Linux
./build/JadeFX\ Tabs.app/Contents/MacOS/JadeFX\ Tabs     # macOS
./build/jadefx-tabs                                      # Linux
./build/JadeFX\ Tree.app/Contents/MacOS/JadeFX\ Tree     # macOS
./build/jadefx-tree                                      # Linux
./build/JadeFX\ Lists.app/Contents/MacOS/JadeFX\ Lists   # macOS
./build/jadefx-lists                                     # Linux
./build/JadeFX\ Split.app/Contents/MacOS/JadeFX\ Split   # macOS
./build/jadefx-split                                     # Linux
```

## A window

```cpp
#include "jadefx/jadefx.hpp"

class HelloWorld : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto label = jadefx::make<jadefx::Label>("Hello World");
        stage.setScene(jadefx::make<jadefx::Scene>(label, 320, 240));
    }
};

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<HelloWorld>(), argc, argv);
}
```

Nodes are owned with `std::shared_ptr`. `jadefx::make<T>()` is `std::make_shared`.

A window draws at most 60 frames per second, and 30 while it is unfocused or minimized (`Stage::setMaxFrameRate`, `Stage::setBackgroundFrameRate`; 0 is uncapped). Between frames the loop sleeps in GLFW's event wait and wakes for input, so an idle window uses almost no CPU even where the driver ignores the swap interval. `JADEFX_MAX_FPS=0` (or any number) overrides both caps from the environment.

Layout is in window points, with the origin at the top left. On a Retina display the framebuffer is larger, and drawing uses that scale so edges stay sharp.

## Scene graph

| JadeFX | Header |
| --- | --- |
| `Application`, `MobileApplication` | `jadefx/application/Application.hpp` |
| `Stage`, `Scene` | `jadefx/stage/Stage.hpp`, `jadefx/scene/Scene.hpp` |
| `Parent` | `jadefx/scene/Parent.hpp` |
| `Region`, `Pane`, `StackPane`, `VBox`, `HBox`, `BorderPane`, `GridPane`, `FlowPane` | one header each in `jadefx/scene/layout/` |
| `Label`, `Button`, `ToggleButton`, `RadioButton`, `CheckBox`, `ProgressBar`, `TextField`, `ComboBox`, `ColorPicker`, `ColorChooser`, `DatePicker`, `Slider`, `Spinner`, `Tooltip` | `jadefx/scene/controls/` |
| `MenuItem`, `Menu`, `MenuButton`, `MenuBar`, `Alert` | `jadefx/scene/controls/` |
| `StyledTextArea`, `CodeArea` | `jadefx/scene/controls/StyledTextArea.hpp`, `jadefx/scene/controls/CodeArea.hpp` |
| `Tab`, `TabPane` | `jadefx/scene/controls/Tab.hpp`, `jadefx/scene/controls/TabPane.hpp` |
| `SplitPane` | `jadefx/scene/controls/SplitPane.hpp` |
| `ScrollPane`, `ScrollBar` | `jadefx/scene/controls/ScrollPane.hpp`, `jadefx/scene/controls/ScrollBar.hpp` |
| `TreeItem`, `TreeView` | `jadefx/scene/controls/TreeItem.hpp`, `jadefx/scene/controls/TreeView.hpp` |
| `ListView`, `ListCell`, `TextFieldListCell` | `jadefx/scene/controls/ListView.hpp` |
| `TableView`, `TableColumn`, `TableCell`, `TextFieldTableCell` | `jadefx/scene/controls/TableView.hpp`, `jadefx/scene/controls/TableColumn.hpp` |
| `IndexedCell`, `ItemCell`, `RowViewBase`, `MultipleSelectionModel`, `ItemSelectionModel`, `FocusModel` | `jadefx/scene/controls/IndexedCell.hpp`, `jadefx/scene/controls/ItemCell.hpp`, `jadefx/scene/controls/RowViewBase.hpp`, `jadefx/scene/controls/SelectionModel.hpp` |
| `ObservableList`, `StringConverter` | `jadefx/collections/ObservableList.hpp`, `jadefx/util/StringConverter.hpp` |
| `Image`, `ImageView` | `jadefx/scene/image/Image.hpp`, `jadefx/scene/image/ImageView.hpp` |
| `Font`, `Color`, `Pos`, `Side`, `Insets` | `jadefx/scene/text/Font.hpp`, `jadefx/paint/Color.hpp`, `jadefx/geometry/Geometry.hpp` |

`VBox` and `HBox` stack children with `setSpacing`. `StackPane` layers them and aligns each child; a child larger than the pane shrinks to fit it, down to the child's minimum size, so a `ScrollPane` in a window scrolls instead of growing past it. `BorderPane` places `setTop`, `setBottom`, `setLeft`, `setRight`, and `setCenter`. `GridPane` places children in cells with `add(child, column, row, columnSpan, rowSpan)` (`GridPane::REMAINING` spans to the last column or row), `addRow`, and `addColumn`, or with the static setters (`setColumnIndex`, `setRowSpan`, `setHalignment`, `setHgrow`, `setFillWidth`, `setMargin`, and so on), which store the constraint on the child in `getProperties()` as JavaFX does. `getColumnConstraints` and `getRowConstraints` take `ColumnConstraints` and `RowConstraints` with fixed, min/pref/max, or percent sizes, a grow `Priority` (`Always`, `Sometimes`, `Never`), a default alignment, and fill; extra space goes to `Always` tracks first, then `Sometimes`, and a narrow pane shrinks tracks toward their minimums. `setGridLinesVisible` outlines the cells. `FlowPane` wraps children into rows (or columns, with `Orientation::Vertical`) at the pane's edge, with `setHgap`, `setVgap`, `setRowValignment`, `setColumnHalignment`, and a preferred size that wraps at `setPrefWrapLength` (400). On either pane the CSS `gap`, `row-gap`, and `column-gap` properties win over `setHgap` and `setVgap`. `Label` measures its text with the bundled Open Sans face. `Label`, `Button`, and the other labeled controls take a graphic node with `setGraphic`: `setContentDisplay` puts it left of the text (the default), right, above, below, behind (`Center`), or alone (`GraphicOnly`), `setGraphicTextGap` spaces them (4 points), and text that no longer fits beside the graphic ends in an ellipsis. `"Google Sans"` is accepted as a family name and uses that same face.

`TreeView` is the material tree from JFoenix's `JFXTreeView`. A `TreeItem` is not a node: `setValue` is the row label, `setGraphic` is an optional node beside it, and `getChildren` holds nested items. `setExpanded` shows or hides those children. Rows indent by `setIndent` (10px per level). A branch draws a disclosure arrow that turns down while it is open; a click on the arrow, a second click on the row, or the Right and Left keys opens and closes it. The selected row keeps a 3px bar on its left edge (`selection-bar`, red unless `setSelectionBarColor` or CSS changes it). The wheel and trackpad scroll in pixels when the rows are taller than the view, so a slow swipe moves the list. The scroll bar is a `ScrollBar`: drag the thumb, or click the track to page. `setShowRoot(false)` hides the root and lists its children, which stay hidden while the root is collapsed. The row type is `tree-cell`, so `tree-cell:selected` and `tree-cell:hover` style it. The label is `tree-cell-label` and the arrow is `tree-disclosure-node`.

Row drag and drop is opt-in per tree, the way JavaFX leaves it to a cell factory's `onDragDetected`, `onDragOver`, and `onDragDropped`. It stays off until `setOnItemsDropped` gets a handler, so other trees keep click-and-drag doing nothing. A left press that moves past a few points (`MouseEvent::stillSincePress`, JavaFX's `isStillSincePress`) starts the drag; a selected row carries the whole selection. The top and bottom quarters of a row are `TreeDropPosition::Before` and `After`, shown as a line with a ring at the row's indent; the middle half is `Into`, shown as a box around the row. Under a branch's last row the pointer's distance from the left picks which ancestor the line closes. The rows scroll near the top and bottom edges, a closed branch held under `Into` opens after 0.7 seconds, and Escape cancels. A row never lands inside itself or its descendants, and `setDropAcceptor` can refuse more. The view never moves items itself: the handler gets a `TreeDrop` (items, target, position, and `parent()`) and changes the model.

`ListView<T>` is OpenJFX's ListView. `getItems()` is an `ObservableList<T>`, and a change to it shows at the next layout; `setItems` can share one list between views. Only the rows on screen have cells, so ten thousand rows scroll as fast as ten. The default cell shows a `Node` item as its graphic and anything else as text (`std::string` as itself, other types through `operator<<`). `setCellFactory` supplies `ListCell<T>` subclasses whose `updateItem(item, empty)` draws a row. `getSelectionModel()` is a `MultipleSelectionModel` with `getSelectedItem` and `getSelectedItems`; `setSelectionMode(SelectionMode::Multiple)` adds Ctrl or Command and a click to toggle a row and Shift and a click for a range. The arrow keys, Home, End, Page Up, and Page Down move the selection, Shift extends it, Ctrl or Command moves the focus alone, Ctrl or Command and Space toggles the focused row, and Ctrl or Command and A selects all. `setEditable(true)` with `TextFieldListCell<T>::forListView()` edits a row on a double-click or F2: Enter commits through the cell's `StringConverter<T>`, and Escape or leaving the field cancels. `setOnEditCommit` decides what a commit stores; without it the new value replaces the item. `setPlaceholder` shows a node while the list is empty, `setFixedCellSize` fixes the row length instead of measuring the first row on screen, and `setOrientation(Orientation::Horizontal)` runs the rows left to right. The view type is `listview` and a row is `list-cell`, with `:selected`, `:empty`, `:focus-visible` on the focused row while the list has focus, and `:nth-child()` counted by row, so `list-cell:nth-child(even)` stripes the rows even as cells are reused.

`TableView<S>` is OpenJFX's TableView, built on the same rows, selection, and keyboard as `ListView`. `getColumns()` holds `TableColumn<S, T>`s: `setCellValueFactory` reads a column's value out of a row item, `setCellFactory` draws its cells (the default shows the value as text), and `setComparator` orders it (the default is `operator<`). A header click sorts by its column, a second click reverses, and a third stops; Shift and a click sorts by several columns, and `getSortOrder`, `setSortOrder`, and `sort` do the same from code. The sort reorders the items and keeps the selection on the same items. Dragging a header's right edge resizes its column between `setMinWidth` and `setMaxWidth`, and dragging a header moves its column. `setColumnResizePolicy(ColumnResizePolicy::Constrained)` fits the columns to the table's width in proportion to `setPrefWidth`; the default, `Unconstrained`, scrolls sideways instead, with the header following the rows. `setVisible(false)` hides a column, and `setSortable`, `setResizable`, `setReorderable`, and `setEditable` turn those gestures off per column. With `setEditable(true)` on the table, a double-click or F2 edits a cell through a cell such as `TextFieldTableCell<S, T>::forTableColumn()`; Left and Right pick the column F2 edits. A commit goes to the column's `setOnEditCommit`, or with no handler to `setCellValueSetter`, which writes the value into a copy of the row item and replaces it in the list. An empty table shows "No content in table" until `setPlaceholder` replaces it. The parts use HTML's names as their types: the table is `table`, its header row `thead`, a header `th`, a row `tr`, and a cell `td`, so `tr:nth-child(even)` stripes rows and `th` styles headers. A sorted header is `:sorted` with `:ascending` or `:descending`. The JavaFX names `table-view`, `column-header`, `table-row-cell`, and `table-cell` are classes on the same nodes.

`ObservableList` takes any number of `addListener` callbacks, which hear each item added, removed, or replaced (`set`) with its index; `setAll` replaces the contents. `MouseEvent::clickCount` counts left presses less than 0.4 seconds and 4 points apart, as JavaFX's `getClickCount` does. `setOnFocusChanged` runs when a node takes or loses the focus. When the window loses the system focus (`Scene::noteWindowFocus`, fed by the GLFW focus callback), the focus owner keeps its place but loses the focus as JavaFX's does: it hears `handleFocusLost` and a change to false, and the reverse when the window returns. Held modifier keys are forgotten and popups that hide on an outside press close. Controls react to losing the focus: a button armed by Space (which, as on the web, fires on release) disarms, a `TextField` drops its selection, an editable `ComboBox` or `DatePicker` commits its text, a `Spinner` commits its editor, a list or table cell editor cancels, and a combo box, color picker, or date picker closes its popup when the focus moves outside it and its popup. Drag and drop follows JavaFX. A press that moves past 4 points runs `setOnDragDetected` (and `handleDragDetected`) on the pressed node and its ancestors; a handler calls `startDragAndDrop(TransferModes)` and fills the returned `Dragboard`: `putString`, `putUrl`, `putFiles`, or `put` with any MIME type, as the web's `DataTransfer` keys its data, and optionally `setDragView` with an `Image` or any node, drawn translucent under the pointer. The node under the pointer then hears `setOnDragEntered`, `setOnDragOver`, and `setOnDragExited`; over calls `acceptTransferModes` to take the drag (Move first, Copy with the shortcut key held, Link with Shift as well), and the cursor shows copy, link, or not-allowed. Releasing there runs `setOnDragDropped`, which calls `setDropCompleted`, and the source hears `setOnDragDone` with the mode performed. Escape, or the window losing the focus, cancels. Files dropped on the window from the system arrive the same way, with no source, through `Scene::noteFileDrop` and the GLFW drop callback. `make controls` has a chip to drag onto a drop zone that also takes files. `applyCss` resolves a node's style at once instead of at the next layout, for a control that creates and measures nodes during layout.

`Image::load` decodes a PNG, JPEG, GIF, or BMP into a bitmap. `ImageView` draws that bitmap in its box. The preferred size is the bitmap size in points, and the bitmap stretches when the view is given a different size. `TreeItem::setGraphic` accepts an `ImageView`, so a row can show an icon beside its label.

`CodeArea`, `StyledTextArea`, `StyleClassedTextArea`, and `InlineCssTextArea` are a virtualized rich text editor in the shape of [RichTextFX](https://github.com/FXMisc/RichTextFX). Text is a list of paragraphs, a newline counts as one code point, and only the visible paragraphs are drawn. `make rich` opens a code page (line numbers, syntax colors, a fold) and a notes page (color, size, bold, underline). Cmd/Ctrl with Z, Y, A, C, X, and V are undo, redo, select all, copy, cut, and paste. On the notes page, Cmd/Ctrl+B toggles bold and Cmd/Ctrl+U toggles underline. The code page pastes characters and leaves color to the highlighter. Alt-click adds a caret. Text whose `TextStyle` has an `href` is a link, as an HTML `a` element: `setLink(start, end, href)` links a range and keeps its other styling (an empty href removes the link), and `linkAt` and `linkRange` read links back. A link takes `--link-color` unless its span sets a fill, and underlines while the pointer is on it (`setLinkUnderline` picks `Hover`, `Always`, or `Never`); every run with the same href hovers together. A click that does not drag reports `LinkEvent{href, range}` to `setOnLinkClicked`. A read-only area follows links on a plain click; an editable one needs Cmd/Ctrl held, as code editors do, and pressing the key over a link lights it up without moving the pointer. Restyling with `setStyleClass` or inline CSS keeps links, and typing at a link's edge does not extend it.

`make controls` opens a window of the form controls. `Button` fires `setOnAction` on a click inside the button, or on Enter or Space while it is focused. `ToggleButton` stays selected; `RadioButton` does not turn off, and a `ToggleGroup` keeps only one of its toggles selected. `CheckBox` toggles checked. `setAllowIndeterminate` cycles unchecked, indeterminate, and checked; an indeterminate box draws a dash. `:selected`, `:determinate`, and `:indeterminate` follow that state. `TextField` is a single line: arrows and Home/End move the caret, Up goes to the start and Down to the end, the selection goes when focus leaves the field, Shift extends the selection, and Enter fires the action. `ComboBox` lists strings; `setEditable(true)` adds a text field. `ComboBox`, `ColorPicker`, and `DatePicker` share `ComboBoxBase`, as in JavaFX: a press toggles the popup, Space, F4, and Alt+Down open it, and Escape or a press outside closes it; `setOnShowing` and `setOnHidden` report both. `setEditable(true)` puts a `TextField` left of the arrow, and `setPromptText` shows while the value is empty. `ColorPicker` is a swatch and hex code that open a `ColorChooser`, laid out like Paint.NET's color window: a hue and saturation wheel dimmed by brightness, a before-and-after swatch (click the old half to go back), a preset palette (`getPresets`), recent colors (`getRecentColors`), and red, green, blue, hue, saturation, value, and alpha sliders over their own gradients, each with a number box, plus a hex field that takes `#rgb`, `#rrggbb`, or `#rrggbbaa` with or without the `#`. The value follows the chooser live (`setOnValueChanged`); Enter or a press outside keeps it, adds it to the recent colors, and fires the action, and Escape restores the old color. A gray keeps its hue, so bringing saturation back returns to it. The chooser's parts carry the classes `.red`, `.green`, `.blue`, `.hue`, `.saturation`, `.brightness`, `.alpha`, and `.hex`, and `ColorChooser` works on its own in a dialog or panel. `DatePicker` holds an optional `LocalDate` (a java.time-shaped date with ISO `yyyy-mm-dd` text, epoch days, month arithmetic that clamps the day, and ISO week numbers). Its field is editable and commits typed text through `setConverter` on Enter or when focus leaves; unreadable text is put back and an empty field is no date. The calendar pages months and years with its arrows; while it is open the arrow keys move a day or a week, Page Up and Page Down a month (a year with Shift), and Enter or a click chooses. `setFirstDayOfWeek` (Sunday by default), `setShowWeekNumbers`, and `setDayCellFactory` shape it; a `DateCell` subclass that overrides `updateItem` can disable or style days. Cells carry `.day-cell`, `.today`, `.previous-month`, and `.next-month`, and the value's day matches `:selected`. `Color::hsb`, `getHue`, `getSaturation`, `getBrightness`, and `toHex` convert colors as JavaFX's `Color` does. `Slider` runs from `getMin` to `getMax`. Dragging the thumb sets `isValueChanging` until release, and a click on the track jumps to that point. Left and Right move a horizontal slider; Up and Down move a vertical one, with Up toward max. Home and End run on key release. `setSnapToTicks` aligns `adjustValue`, the track, and the keys to the tick grid. Tick marks and labels sit under a horizontal track and to the right of a vertical one. `Spinner` steps a `SpinnerValueFactory`: `IntegerSpinnerValueFactory`, `DoubleSpinnerValueFactory`, or `ListSpinnerValueFactory`. The arrow buttons and the arrow keys step the value, and holding a button repeats. `setEditable(true)` commits the editor on Enter and when focus leaves it. The default stacks both arrows on the right; `arrows-on-left-vertical`, `arrows-on-left-horizontal`, `arrows-on-right-horizontal`, `split-arrows-vertical`, and `split-arrows-horizontal` select the other layouts. `Tooltip::install` shows text after the pointer rests on a node. `MenuBar` and `MenuButton` open `MenuItem` rows, including submenus and Ctrl/Command accelerators. `Alert` is a modal dialog. The arrow keys move focus between its buttons, Enter presses the focused one, and Escape presses Cancel. `show` leaves it up until a button is chosen; `showAndWait` pumps the window when a frame is not already running. `setDisable` blocks input, and `:disabled` matches that state.

`ProgressBar` shows progress from 0 to 1. A negative value, including `ProgressBar::INDETERMINATE_PROGRESS`, is indeterminate: a short bar travels the track and turns around. `setProgress(0.25)` fills a quarter of the track. A value above 1 fills the track, and `getProgress` still returns that value. The preferred width is 100. The preferred height is the fill's height; the fill's padding defaults to `0.75em`, so the bar is `1.5em` tall. The maximum size starts at that preferred size, so the bar stays put until `setPrefWidth`, `setMaxWidth`, or CSS gives it a size. The fill width snaps to half a pixel. The control type is `progress-bar`, the groove is `track`, and the fill is `bar`. `:determinate` and `:indeterminate` follow the progress. `indeterminate-bar-length`, `indeterminate-bar-escape`, `indeterminate-bar-flip`, and `indeterminate-bar-animation-time` style the traveling bar. They default to 60px, escaping past both ends, flipping at each end, and 2 seconds each way.

`SplitPane` places two or more nodes in a row, or in a column, with a divider between each pair. Each item fills the space on its side of the divider. `getItems()` is that list. `setOrientation` switches between horizontal and vertical. `setDividerPosition` and `setDividerPositions` take fractions from 0 to 1. The pane keeps each divider inside the neighboring items' minimum and maximum sizes, so the fraction read back can differ from the fraction that was set. Dragging a divider updates the fraction. `setResizableWithParent(node, false)` keeps that item's size when the pane is resized. The divider type is `split-pane-divider`. `:horizontal` and `:vertical` follow the orientation. The grip is `horizontal-grabber` or `vertical-grabber`. Divider thickness is its left padding plus its right padding.

`ScrollPane` shows one node through a clipped viewport, as in JavaFX. `setContent` sets the node, which keeps its preferred size unless `setFitToWidth` or `setFitToHeight` stretches it to the viewport. `setHvalue` and `setVvalue` place the view between `hmin`/`hmax` and `vmin`/`vmax` (0 to 1 by default), and they keep their place when the content or the pane resizes; `setOnHvalueChanged` and `setOnVvalueChanged` report every move. `setHbarPolicy` and `setVbarPolicy` take `ScrollBarPolicy::AsNeeded` (the default), `Always`, or `Never`. `setPrefViewportWidth` and `setPrefViewportHeight` set the size the pane asks for. The wheel scrolls 40 points a notch, sideways with Shift or a sideways swipe. When the pane cannot move any further, the wheel event goes on to its ancestors, so a pane inside another hands the scroll over at its ends. With focus inside, the arrow keys scroll 20 points, Page Up and Page Down a viewport, and Home and End to the top and bottom. Content outside the viewport is neither drawn nor clickable. The pane type is `scroll-pane` and its viewport is `viewport`.

`ScrollBar` is the bar `ScrollPane` and `TreeView` use, and it works on its own. `value` runs from `min` to `max`, and `visibleAmount` sets the thumb's share of the track. A press on the thumb drags it; a press on the track moves by `blockIncrement`. The arrow keys move by `unitIncrement`, Page Up and Page Down by `blockIncrement`, and Home and End to the ends. `setOnValueChanged` reports each change and `isValueChanging` is true while the thumb is dragged. The type is `scroll-bar`, with `:horizontal` or `:vertical`. `ScrollTrack` is the geometry under it, for a control that draws its own bar, as `StyledTextArea` does.

`setFocusTraversable(false)` on a node sends the focus a press would give it to its nearest ancestor that takes focus, as JavaFX does for a control's own scroll bars.

`TabPane` shows one `Tab` page at a time. A tab is not a node: `setText` is the header title and `setContent` is the page. The selected page fills the area beside the header strip. `setSide` puts that strip on the top, bottom, left, or right, and left and right titles stay horizontal. `setTabClosingPolicy` draws close buttons on the selected tab (the default), on every closable tab, or not at all. A close click calls `onCloseRequest`; `consume()` keeps the tab, and `onClosed` runs after it leaves the pane. `select` or a header click changes the page. Headers shrink to fit the strip, and a long title ends in an ellipsis. The header type is `tab`, so `tab:selected` and `tab:hover` style it. The title node is `tab-label`, and the close mark is `tab-close-button`.

Buttons in the Purple sample are styled stack panes, the same way the Java library builds them:

```cpp
auto button = jadefx::make<jadefx::StackPane>();
button->getClassList().add("test-button");
button->getChildren().add(jadefx::make<jadefx::Label>("Get started"));
button->setOnMouseClicked([](const jadefx::MouseEvent&) {
    std::printf("Clicked test button!\n");
});
```

## Stylesheets

`setStylesheet` parses a CSS subset. Selectors can be a type (`scene`, `label`, `button`, `togglebutton`, `radiobutton`, `checkbox`, `progress-bar`, `track`, `bar`, `textfield`, `combobox`, `combo-row`, `color-picker`, `color-chooser`, `color-wheel`, `color-swatch`, `date-picker`, `date-picker-popup`, `date-cell`, `slider`, `spinner`, `increment-arrow-button`, `decrement-arrow-button`, `tooltip`, `menubar`, `menu`, `menubutton`, `menu-item`, `separator`, `alert`, `vbox`, `stackpane`, `borderpane`, `gridpane`, `flowpane`, `pane`, `tabpane`, `tab`, `tab-label`, `tab-close-button`, `tab-header-area`, `treeview`, `tree-cell`, `tree-cell-label`, `listview`, `list-cell`, `scroll-pane`, `scroll-bar`, `viewport`, `tree-disclosure-node`, `selection-bar`, `split-pane`, `split-pane-divider`, `horizontal-grabber`, `vertical-grabber`), a universal `*`, a class (`.test-button`), an id (`#SignUp`), and the pseudos `:hover`, `:active`, `:focus`, `:focus-within`, `:disabled`, `:select` (also written `:selected`), and `:nth-child()` with `odd`, `even`, a number, or `An+B`. A node shown as a popup matches `:popover-open`, as an HTML popover does. Any other pseudo-class, such as `:horizontal`, `:vertical`, `:determinate`, `:indeterminate`, `:empty`, or `:focus-visible`, matches a state a control sets with `setPseudoState`. A space is a descendant combinator and `>` is a child combinator.

Supported properties: `width`, `height`, `min-*`, `max-*`, `padding`, `spacing`, `gap`, `row-gap`, `column-gap`, `alignment`, `orientation` (`horizontal` or `vertical`), `color`, `font-size`, `font-family`, `background-color`, `background-image` (`linear-gradient`, including `to bottom` and extra color stops), `border-radius`, `border-width`, `border-color`, `border-style`, `box-shadow`, `opacity`, `cursor`, `transition`, `indeterminate-bar-length`, `indeterminate-bar-escape`, `indeterminate-bar-flip`, and `indeterminate-bar-animation-time`. Lengths accept `px`, `em`, `%`, and `calc(100% - 48px)`. An unknown unit is ignored rather than treated as pixels.

`color`, `font-size`, `font-family`, and `cursor` inherit. `background-color` stays behind a gradient instead of replacing it. A transition on `background-color`, `background-image`, `color`, `border-color`, `border-width`, or `box-shadow` fades those values. A timing function such as `ease` is accepted and does not cancel the duration. Other properties take their new value on the next frame.

`cursor` changes the mouse cursor while the pointer is over a node. Buttons, check boxes, toggles, menus, tabs, combo rows, and tree rows use `pointer` (the hand). Text fields and text areas use `text` (the I-beam). A text area's scrollbar keeps the arrow. `auto` keeps that control default, and `default` is the arrow. A disabled control drops its hand or I-beam unless a rule sets another cursor. `setCursor` sets the same value from code; a stylesheet replaces it.

Keywords: `auto`, `default`, `pointer` (`hand`), `text` (`vertical-text`), `crosshair` (`cell`), `move` (`all-scroll`), `not-allowed` (`no-drop`), `ew-resize`, `ns-resize`, `nwse-resize`, `nesw-resize` (and the single-edge names such as `e-resize`), `none`, `wait`, `help`, `progress`, `grab` (`open-hand`), `grabbing` (`closed-hand`), `zoom-in`, `zoom-out`, `context-menu`, `alias`, and `copy`. `grab` uses the hand. `wait`, `help`, `progress`, `zoom-in`, `zoom-out`, `alias`, `copy`, and `context-menu` use the arrow. The first recognized keyword in a comma-separated list wins, so `url(missing), pointer` is the hand.

`setStyle("color: white;")` sets inline declarations on one node. Inline declarations win over the stylesheet.

The cascade follows CSS and JavaFX. Lowest first: the user-agent stylesheet (the theme), then colors and fonts set from code (`setBackground`, `setTextFill`, `setFont`), then application stylesheets from the scene down to the node, then inline declarations. Within one layer the more specific selector wins (ids over classes and pseudo-classes over types), and among equals the later rule. `!important` lifts a declaration over every normal one.

Custom properties work as in CSS: `--name: value` declares one, every node inherits it, and `var(--name)` or `var(--name, fallback)` uses it in any value. A `var()` inside a custom property is resolved on the node that declares it. `:root` matches the scene.

## Themes

The look of every control comes from a user-agent stylesheet, as JavaFX's Modena does. Two are built in, light (the default) and dark, and they differ only in the custom properties they set on `:root`, so a theme is a set of colors: `--background-color`, `--surface-color`, `--text-color`, `--muted-color`, `--faint-color`, `--border-color`, `--accent-color`, `--outline-color` (the focus ring), `--link-color` (the `LinkText` system color), `--hover-color`, `--track-color`, `--subtle-color`, `--selection-color`, `--selection-hover-color`, `--row-hover-color`, `--tab-color`, `--tooltip-color`, `--tooltip-text-color`, `--dimmer-color`, `--wash-color`, `--text-selection-color`, `--current-line-color`, `--gutter-color`, `--scrollbar-color`, `--divider-color`, `--grip-color`, `--info-color`, `--warning-color`, `--error-color`, and `--success-color`. `ThemeColor` names each one in C++. A theme color set to `currentColor` takes the node's text color, as in CSS.

```cpp
jadefx::Application::setUserAgentStylesheet(jadefx::Application::STYLESHEET_DARK);  // every scene
scene->setUserAgentStylesheet(jadefx::Theme::DARK);  // one scene; "" follows the application
scene->setStylesheet(":root { --accent-color: #7b1fa2; }");  // retint the theme
scene->setStylesheet("checkbox { accent-color: #188038; }");  // one kind of control
```

`setUserAgentStylesheet` also takes CSS text, to replace the theme outright; `Theme::stylesheet("light")` is the built-in one to start from. The web's `accent-color`, `caret-color`, and `outline-color` set the matching custom properties. A switch restyles the scene at its next layout. Application stylesheets that use the variables, such as `background-color: var(--surface-color)`, follow the theme too; the samples do, and `make controls` has a Dark theme box. A control that draws its own shapes reads a color with `themeColor(ThemeColor::Accent)`.

## Drawing into an existing OpenGL program

Create the context yourself, then drive a stage from the frame loop. Coordinates passed to `pushMove` and `pushButton` are window points, not framebuffer pixels.

```cpp
jadefx::Stage stage;
stage.initializeGraphics(procAddress);   // glfwGetProcAddress, or the GLFM equivalent
stage.getScene().setRoot(root);

// each frame, after the context is current:
stage.pushMove(mouseX, mouseY);
stage.frame(windowWidth, windowHeight, framebufferWidth, framebufferHeight);
```

`frame` lays out the scene, draws it, and leaves the GL state with blending enabled. Call `shutdownGraphics` before destroying the context. `setCursorHandler` reports the cursor for the pointer after each frame. `cursorShape` maps that value onto a system cursor such as the hand or the I-beam. A window opened with `Application::launch` applies it on its own.

## iOS and Android

Configure with the iOS or Android CMake toolchain. Those builds compile GLFM and define `JADEFX_GLFM`. The process entry is `glfmMain`, which calls `createApplication()` from `examples/purple_app.cpp`.

```sh
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
```

`android/AndroidManifest.xml` is a NativeActivity manifest whose library name is `jadefx-purple`. Open Sans is embedded in the binary. Shader files live in `res/shaders` and are loaded from `shaders/` next to the program (on macOS, `Contents/Resources/shaders`; on Android, `assets/shaders`). If one is missing, the app shows an error and quits. `MobileApplication::showStatusBar`, `setOrientation`, and `setMultitouchEnabled` are applied through GLFM. On the desktop they are stored and otherwise ignored, and the window uses a 375×667 phone size.

## Layout of the source

`include/jadefx` is the public API. `res` holds the shader files and the Open Sans face. `src/scene` is the scene graph and does not call OpenGL. `src/gl` is the loader, the rounded-rectangle shader, and the font atlas. `src/platform` is the GLFW host and the GLFM host.
