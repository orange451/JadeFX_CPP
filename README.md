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
```

A system GLFW is used when CMake can find it. Otherwise CMake downloads GLFW 3.5.1. Linux needs the X11 and Wayland development packages to build that copy.

There is also a smaller window, a BorderPane sample (`make border`), a TabPane sample (`make tabs`), and a TreeView sample (`make tree`):

```sh
./build/JadeFX\ Hello.app/Contents/MacOS/JadeFX\ Hello   # macOS
./build/jadefx-hello                                     # Linux
./build/JadeFX\ Border.app/Contents/MacOS/JadeFX\ Border # macOS
./build/jadefx-border                                    # Linux
./build/JadeFX\ Tabs.app/Contents/MacOS/JadeFX\ Tabs     # macOS
./build/jadefx-tabs                                      # Linux
./build/JadeFX\ Tree.app/Contents/MacOS/JadeFX\ Tree     # macOS
./build/jadefx-tree                                      # Linux
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

Layout is in window points, with the origin at the top left. On a Retina display the framebuffer is larger, and drawing uses that scale so edges stay sharp.

## Scene graph

| JadeFX | Header |
| --- | --- |
| `Application`, `MobileApplication` | `jadefx/application/Application.hpp` |
| `Stage`, `Scene` | `jadefx/stage/Stage.hpp`, `jadefx/scene/Scene.hpp` |
| `Parent` | `jadefx/scene/Parent.hpp` |
| `Region`, `Pane`, `StackPane`, `VBox`, `HBox`, `BorderPane` | one header each in `jadefx/scene/layout/` |
| `Label` | `jadefx/scene/Controls/Label.hpp` |
| `StyledTextArea`, `CodeArea` | `jadefx/scene/Controls/StyledTextArea.hpp`, `jadefx/scene/Controls/CodeArea.hpp` |
| `Tab`, `TabPane` | `jadefx/scene/Controls/Tab.hpp`, `jadefx/scene/Controls/TabPane.hpp` |
| `TreeItem`, `TreeView` | `jadefx/scene/Controls/TreeItem.hpp`, `jadefx/scene/Controls/TreeView.hpp` |
| `Font`, `Color`, `Pos`, `Side`, `Insets` | `jadefx/scene/text/Font.hpp`, `jadefx/paint/Color.hpp`, `jadefx/geometry/Geometry.hpp` |

`VBox` and `HBox` stack children with `setSpacing`. `StackPane` layers them and aligns each child. `BorderPane` places `setTop`, `setBottom`, `setLeft`, `setRight`, and `setCenter`. `Label` measures its text with the bundled Open Sans face. `"Google Sans"` is accepted as a family name and uses that same face.

`TreeView` is the material tree from JFoenix's `JFXTreeView`. A `TreeItem` is not a node: `setValue` is the row label, `setGraphic` is an optional node beside it, and `getChildren` holds nested items. `setExpanded` shows or hides those children. Rows indent by `setIndent` (10px per level). A branch draws a disclosure arrow that turns down while it is open; a click on the arrow, a second click on the row, or the Right and Left keys opens and closes it. The selected row keeps a 3px bar on its left edge (`selection-bar`, red unless `setSelectionBarColor` or CSS changes it). The wheel and trackpad scroll in pixels when the rows are taller than the view, so a slow swipe moves the list. The scrollbar is the same one the rich text editor uses: drag the thumb, or click the track to page. `setShowRoot(false)` hides the root and lists its children, which stay hidden while the root is collapsed. The row type is `tree-cell`, so `tree-cell:selected` and `tree-cell:hover` style it. The label is `tree-cell-label` and the arrow is `tree-disclosure-node`.

`CodeArea`, `StyledTextArea`, `StyleClassedTextArea`, and `InlineCssTextArea` are a virtualized rich text editor in the shape of [RichTextFX](https://github.com/FXMisc/RichTextFX). Text is a list of paragraphs, a newline counts as one code point, and only the visible paragraphs are drawn. `make rich` opens a code page (line numbers, syntax colors, a fold) and a notes page (color, size, bold, underline). Cmd/Ctrl with Z, Y, A, C, X, V, B, and U are undo, redo, select all, copy, cut, paste, bold, and underline. Alt-click adds a caret.

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

`setStylesheet` parses a CSS subset. Selectors can be a type (`scene`, `label`, `vbox`, `stackpane`, `borderpane`, `pane`, `tabpane`, `tab`, `tab-label`, `tab-close-button`, `tab-header-area`, `treeview`, `tree-cell`, `tree-cell-label`, `tree-disclosure-node`, `selection-bar`), a universal `*`, a class (`.test-button`), an id (`#SignUp`), and the pseudos `:hover`, `:active`, `:focus`, `:focus-within`, and `:select` (also written `:selected`). A space is a descendant combinator and `>` is a child combinator.

Supported properties: `width`, `height`, `min-*`, `max-*`, `padding`, `spacing`, `alignment`, `color`, `font-size`, `font-family`, `background-color`, `background-image` (`linear-gradient`, including `to bottom` and extra color stops), `border-radius`, `border-width`, `border-color`, `border-style`, `box-shadow`, `opacity`, and `transition`. Lengths accept `px`, `em`, `%`, and `calc(100% - 48px)`. An unknown unit is ignored rather than treated as pixels.

`color`, `font-size`, and `font-family` inherit. `background-color` stays behind a gradient instead of replacing it. A transition on `background-color`, `background-image`, `color`, `border-color`, `border-width`, or `box-shadow` fades those values. A timing function such as `ease` is accepted and does not cancel the duration. Other properties take their new value on the next frame.

`setStyle("color: white;")` sets inline declarations on one node. Inline declarations win over the stylesheet.

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

`frame` lays out the scene, draws it, and leaves the GL state with blending enabled. Call `shutdownGraphics` before destroying the context.

## iOS and Android

Configure with the iOS or Android CMake toolchain. Those builds compile GLFM and define `JADEFX_GLFM`. The process entry is `glfmMain`, which calls `createApplication()` from `examples/purple_app.cpp`.

```sh
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
```

`android/AndroidManifest.xml` is a NativeActivity manifest whose library name is `jadefx-purple`. Shaders and Open Sans are embedded in the binary, so the phone build does not need a separate asset copy. `MobileApplication::showStatusBar`, `setOrientation`, and `setMultitouchEnabled` are applied through GLFM. On the desktop they are stored and otherwise ignored, and the window uses a 375×667 phone size.

## Layout of the source

`include/jadefx` is the public API. `src/scene` is the scene graph and does not call OpenGL. `src/gl` is the loader, the rounded-rectangle shader, and the font atlas. `src/platform` is the GLFW host and the GLFM host.
