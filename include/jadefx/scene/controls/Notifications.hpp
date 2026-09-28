#pragma once

#include "jadefx/event/Events.hpp"
#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/scene/controls/Alert.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jadefx {

class Node;

// Cards that slide into a corner of the window and leave on their own, in the
// shape of ControlsFX's Notifications builder:
//
//     Notifications::create().title("Saved").text("scene.json").owner(*button).showInformation();
//
// A card has the type's icon (or a graphic), a title, text split into lines at
// '\n', optional action buttons, and a close button. It hides after hideAfter
// seconds, which a pointer resting on it holds off; zero or less keeps it until
// closed. Cards stack from their position's corner, newest nearest the edge, and
// the others slide over when one leaves. A click on the card, outside its
// buttons, runs onAction and closes it. Cards are .notification with a class for
// the type (.information, .warning, .error, .confirmation), holding .title,
// .text, .actions, and .close-button.
class Notifications {
public:
    static Notifications create() { return Notifications(); }

    Notifications& title(std::string text);
    Notifications& text(std::string text);
    // Replaces the type's icon.
    Notifications& graphic(std::shared_ptr<Node> graphic);
    // Where the cards stack. The default is the bottom right.
    Notifications& position(Pos position);
    Notifications& hideAfter(double seconds);
    Notifications& onAction(ActionHandler handler);
    // Adds a button. Pressing it runs the handler and closes the card.
    Notifications& action(std::string label, ActionHandler handler);
    Notifications& hideCloseButton();
    Notifications& styleClass(std::string name);
    // The node whose scene shows the card. Without one, show does nothing.
    Notifications& owner(Node& node);

    void show();
    void showInformation();
    void showWarning();
    void showError();
    void showConfirm();

    // Closes every notification and toast in the owner's scene.
    static void hideAll(Node& owner);

private:
    void show(AlertType type);

    std::string title_;
    std::string text_;
    std::shared_ptr<Node> graphic_;
    Pos position_ = Pos::BottomRight;
    double hideAfter_ = 5.0;
    ActionHandler onAction_;
    std::vector<std::pair<std::string, ActionHandler>> actions_;
    std::vector<std::string> styleClasses_;
    bool closeButton_ = true;
    Node* owner_ = nullptr;
};

// A short line of text that fades in and leaves after a moment, as an Android
// toast. It takes no input: clicks pass through it. It sits at the bottom
// center unless position, like an Android toast's gravity, puts it elsewhere;
// toasts stack from their position's corner as notification cards do.
// A toast is a .toast label.
class Toast {
public:
    static constexpr double LENGTH_SHORT = 2.0;
    static constexpr double LENGTH_LONG = 3.5;

    static void show(Node& owner, std::string text, double seconds = LENGTH_SHORT,
                     Pos position = Pos::BottomCenter);
};

}  // namespace jadefx
