#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

class Image;
class Node;

// What a drop does with the data, as JavaFX's TransferMode and the HTML dropEffect.
enum class TransferMode : std::uint8_t { Copy = 1, Move = 2, Link = 4 };

// A set of transfer modes: what a source allows or a target accepts.
class TransferModes {
public:
    constexpr TransferModes() = default;
    constexpr TransferModes(TransferMode mode) : bits_(static_cast<std::uint8_t>(mode)) {}

    static constexpr TransferModes none() { return TransferModes(); }
    static constexpr TransferModes any() { return TransferModes(7); }
    static constexpr TransferModes copyOrMove() { return TransferModes(3); }

    constexpr bool contains(TransferMode mode) const { return (bits_ & static_cast<std::uint8_t>(mode)) != 0; }
    constexpr bool empty() const { return bits_ == 0; }
    constexpr TransferModes operator|(TransferModes other) const { return TransferModes(bits_ | other.bits_); }
    constexpr TransferModes operator&(TransferModes other) const { return TransferModes(bits_ & other.bits_); }
    constexpr bool operator==(TransferModes other) const { return bits_ == other.bits_; }

private:
    constexpr explicit TransferModes(int bits) : bits_(static_cast<std::uint8_t>(bits)) {}

    std::uint8_t bits_ = 0;
};

constexpr TransferModes operator|(TransferMode a, TransferMode b) { return TransferModes(a) | TransferModes(b); }

// Data by format, as JavaFX's ClipboardContent and the HTML DataTransfer. Formats
// are MIME types, as on the web: text/plain for a string and text/uri-list for a
// URL. Files are paths, kept apart from the text formats.
class ClipboardContent {
public:
    static constexpr const char* kPlainText = "text/plain";
    static constexpr const char* kUrl = "text/uri-list";

    void put(const std::string& format, std::string data) { data_[format] = std::move(data); }
    bool has(const std::string& format) const { return data_.count(format) != 0; }
    // Empty when the format is missing.
    std::string get(const std::string& format) const;
    // The formats present, in order, with "Files" last when there are files, as DataTransfer.types lists them.
    std::vector<std::string> getTypes() const;

    void putString(std::string text) { put(kPlainText, std::move(text)); }
    bool hasString() const { return has(kPlainText); }
    std::string getString() const { return get(kPlainText); }
    void putUrl(std::string url) { put(kUrl, std::move(url)); }
    bool hasUrl() const { return has(kUrl); }
    std::string getUrl() const { return get(kUrl); }
    void putFiles(std::vector<std::string> paths) { files_ = std::move(paths); }
    bool hasFiles() const { return !files_.empty(); }
    const std::vector<std::string>& getFiles() const { return files_; }

    bool empty() const { return data_.empty() && files_.empty(); }

private:
    std::map<std::string, std::string> data_;
    std::vector<std::string> files_;
};

// The data a drag carries, in the shape of JavaFX's Dragboard: its content, the
// modes the source allows, and an optional view drawn under the pointer.
class Dragboard : public ClipboardContent {
public:
    explicit Dragboard(TransferModes allowed = TransferModes::any()) : allowed_(allowed) {}

    void setContent(const ClipboardContent& content) { static_cast<ClipboardContent&>(*this) = content; }
    TransferModes getTransferModes() const { return allowed_; }

    // Drawn at the pointer, offset so the pointer sits at (offsetX, offsetY) in it.
    void setDragView(std::shared_ptr<Image> image, double offsetX = 0, double offsetY = 0);
    // Any node as the view, such as a label naming what is dragged.
    void setDragView(std::shared_ptr<Node> view, double offsetX = 0, double offsetY = 0);
    const std::shared_ptr<Node>& getDragView() const { return view_; }
    double getDragViewOffsetX() const { return offsetX_; }
    double getDragViewOffsetY() const { return offsetY_; }

private:
    TransferModes allowed_;
    std::shared_ptr<Node> view_;
    double offsetX_ = 0;
    double offsetY_ = 0;
};

// One step of a drag, in the shape of JavaFX's DragEvent: entered, over, exited,
// dropped, or done. A target accepts the drag during over with acceptTransferModes
// and reports the result of dropped with setDropCompleted; the source learns the
// mode that was performed from done, which is empty when nothing was dropped.
// Over, dropped, and done go to the node under the pointer (the source, for done)
// and then its ancestors until one consumes the event.
struct DragEvent {
    double x = 0;
    double y = 0;
    Dragboard* dragboard = nullptr;
    // The node the drag started from, or null for files dropped from the system.
    Node* gestureSource = nullptr;
    // The node that accepted the drag, once one has.
    Node* gestureTarget = nullptr;
    // The node handling the event.
    Node* target = nullptr;
    // The shortcut key was held: Ctrl, or Command on a Mac.
    bool shortcut = false;
    bool shift = false;
    bool consumed = false;

    // Accepts the drag with the first mode, of those given and allowed by the source,
    // in the order Move, Copy, Link, unless the shortcut key asks for a copy (with Shift,
    // a link). Only for over.
    void acceptTransferModes(TransferModes modes);
    bool isAccepted() const { return accepted_; }
    TransferMode getAcceptedTransferMode() const { return mode_; }
    // The mode the drop performed. Only meaningful for done.
    bool isTransferDone() const { return accepted_; }
    TransferMode getTransferMode() const { return mode_; }
    void setDropCompleted(bool completed) { completed_ = completed; }
    bool isDropCompleted() const { return completed_; }
    Dragboard& getDragboard() const { return *dragboard; }
    void consume() { consumed = true; }

private:
    friend class Scene;

    bool accepted_ = false;
    TransferMode mode_ = TransferMode::Move;
    bool completed_ = false;
};

using DragHandler = std::function<void(DragEvent&)>;

}  // namespace jadefx
