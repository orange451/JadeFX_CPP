#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// Drag and drop between nodes, and files dropped from the system.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

std::shared_ptr<jadefx::Pane> Box(double x, double y, double width, double height) {
    auto box = jadefx::make<jadefx::Pane>();
    box->setPrefSize(width, height);
    box->setTranslateX(x);
    box->setTranslateY(y);
    return box;
}

struct Center {
    double x;
    double y;
};

Center CenterOf(const jadefx::Node& node) {
    return {node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5};
}

// A source that drags "hello", a target that takes text or files, and a box that takes nothing.
struct Rig {
    std::shared_ptr<jadefx::Pane> root = jadefx::make<jadefx::Pane>();
    std::shared_ptr<jadefx::Pane> source = Box(10, 10, 80, 30);
    std::shared_ptr<jadefx::Pane> target = Box(200, 10, 120, 60);
    std::shared_ptr<jadefx::Pane> refuser = Box(10, 200, 120, 60);
    std::shared_ptr<jadefx::Label> view = jadefx::make<jadefx::Label>("hello");
    std::shared_ptr<jadefx::Scene> scene;
    int entered = 0;
    int exited = 0;
    int released = 0;
    std::string dropped;
    std::vector<std::string> files;
    int done = 0;
    bool transferred = false;
    jadefx::TransferMode doneMode = jadefx::TransferMode::Move;

    Rig() {
        // The boxes are translated, so the root is given the room they need.
        root->setPrefSize(400, 300);
        root->getChildren().add(source);
        root->getChildren().add(target);
        root->getChildren().add(refuser);
        scene = jadefx::make<jadefx::Scene>(root, 400, 300);
        scene->layout(400, 300, 0);
        source->setOnDragDetected([this](const jadefx::MouseEvent&) {
            jadefx::Dragboard* board = source->startDragAndDrop(jadefx::TransferMode::Copy | jadefx::TransferMode::Move);
            jadefx::ClipboardContent content;
            content.putString("hello");
            board->setContent(content);
            board->setDragView(view, 4, 4);
        });
        source->setOnMouseReleased([this](const jadefx::MouseEvent&) { ++released; });
        source->setOnDragDone([this](jadefx::DragEvent& event) {
            ++done;
            transferred = event.isTransferDone();
            doneMode = event.getTransferMode();
        });
        target->setOnDragEntered([this](jadefx::DragEvent&) { ++entered; });
        target->setOnDragExited([this](jadefx::DragEvent&) { ++exited; });
        target->setOnDragOver([this](jadefx::DragEvent& event) {
            if (event.gestureSource != target.get() &&
                (event.getDragboard().hasString() || event.getDragboard().hasFiles())) {
                event.acceptTransferModes(jadefx::TransferModes::copyOrMove());
            }
            event.consume();
        });
        target->setOnDragDropped([this](jadefx::DragEvent& event) {
            dropped = event.getDragboard().getString();
            files = event.getDragboard().getFiles();
            event.setDropCompleted(true);
            event.consume();
        });
    }

    void press(const jadefx::Node& node) {
        const Center at = CenterOf(node);
        scene->noteButton(0, true, at.x, at.y, 0);
    }
    void moveTo(const jadefx::Node& node) {
        const Center at = CenterOf(node);
        scene->noteMove(at.x, at.y);
    }
    void release(const jadefx::Node& node) {
        const Center at = CenterOf(node);
        scene->noteButton(0, false, at.x, at.y, scene->modifierMask());
    }
};

void TestDragBetweenNodes() {
    Rig rig;
    Expect(rig.source->startDragAndDrop(jadefx::TransferModes::any()) == nullptr,
           "startDragAndDrop does nothing outside drag-detected");
    rig.press(*rig.source);
    const Center start = CenterOf(*rig.source);
    rig.scene->noteMove(start.x + 2, start.y);
    Expect(!rig.scene->isDragging(), "a small move is not a drag");
    rig.scene->noteMove(start.x + 20, start.y);
    Expect(rig.scene->isDragging(), "moving past the threshold detects the drag");
    Expect(rig.scene->isPopupShowing(rig.view.get()) && rig.view->isMouseTransparent(),
           "the drag view follows the pointer and lets it through");
    Expect(rig.scene->hoverCursor() == jadefx::Cursor::NotAllowed, "nothing accepts over the source");

    rig.moveTo(*rig.target);
    Expect(rig.entered == 1 && rig.exited == 0, "the target hears entered once");
    Expect(rig.scene->hoverCursor() == jadefx::Cursor::Default, "an accepted move shows the arrow");
    rig.scene->noteKey(jadefx::Key::LeftControl, true, false, 0);
    rig.moveTo(*rig.target);
    Expect(rig.scene->hoverCursor() == jadefx::Cursor::Copy, "the shortcut key asks for a copy");
    Expect(rig.entered == 1, "moving within the target does not enter again");
    rig.release(*rig.target);
    rig.scene->noteKey(jadefx::Key::LeftControl, false, false, jadefx::Key::ModControl);
    Expect(!rig.scene->isDragging() && rig.dropped == "hello", "releasing over the target drops the text");
    Expect(rig.done == 1 && rig.transferred && rig.doneMode == jadefx::TransferMode::Copy,
           "the source hears the drop was a copy");
    Expect(rig.exited == 1 && !rig.scene->isPopupShowing(rig.view.get()), "the target is exited and the view goes");
    Expect(rig.released == 0, "the source's mouse release goes to the drag instead");

    // Escape cancels, and a node that does not accept refuses the drop.
    rig.press(*rig.source);
    rig.moveTo(*rig.target);
    rig.scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!rig.scene->isDragging() && rig.done == 2 && !rig.transferred, "Escape cancels the drag");
    rig.release(*rig.target);
    Expect(rig.dropped == "hello" && rig.released == 0, "the release after a cancel drops nothing");

    rig.dropped.clear();
    rig.press(*rig.source);
    rig.moveTo(*rig.refuser);
    rig.release(*rig.refuser);
    Expect(rig.done == 3 && !rig.transferred && rig.dropped.empty(), "a drop where nothing accepts does nothing");

    // A drop handler may remove the source.
    rig.target->setOnDragDropped([&](jadefx::DragEvent& event) {
        event.setDropCompleted(true);
        rig.root->getChildren().removeIf([&](const std::shared_ptr<jadefx::Node>& node) { return node == rig.source; });
    });
    rig.press(*rig.source);
    rig.moveTo(*rig.target);
    rig.release(*rig.target);
    Expect(!rig.scene->isDragging() && rig.done == 3, "a source removed during the drop is not told");
}

void TestFileDrop() {
    Rig rig;
    const Center target = CenterOf(*rig.target);
    const bool taken = rig.scene->noteFileDrop(target.x, target.y, {"/tmp/a.png", "/tmp/b.txt"});
    Expect(taken && rig.files.size() == 2 && rig.files[1] == "/tmp/b.txt", "files dropped from the system arrive");
    Expect(rig.entered == 1 && rig.exited == 1 && rig.done == 0, "entered and exited, and no source to tell");
    const Center refuser = CenterOf(*rig.refuser);
    Expect(!rig.scene->noteFileDrop(refuser.x, refuser.y, {"/tmp/a.png"}), "a node that refuses files leaves them");

    jadefx::ClipboardContent content;
    content.putUrl("https://example.com");
    content.putString("text");
    content.putFiles({"/tmp/c"});
    const std::vector<std::string> types = content.getTypes();
    Expect(types.size() == 3 && types.back() == "Files" && content.get("text/uri-list") == "https://example.com",
           "content keeps its formats by MIME type, with Files last");
}

}  // namespace

int RunDragDropTests() {
    gFailures = 0;
    TestDragBetweenNodes();
    TestFileDrop();
    if (gFailures == 0) {
        std::printf("drag and drop tests passed\n");
    }
    return gFailures;
}
