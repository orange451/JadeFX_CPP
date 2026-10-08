#pragma once

#include "jadefx/event/Events.hpp"

#include <functional>
#include <memory>
#include <string>

namespace jadefx {

class Menu;
class MenuBar;
class Node;

// One entry in a Menu or MenuButton. Not a scene-graph node. fire() runs the
// action unless the item is disabled. Accelerators are matched by menu bars
// and menu buttons while they are in a scene.
class MenuItem {
public:
    MenuItem();
    explicit MenuItem(std::string text);
    virtual ~MenuItem();

    MenuItem(const MenuItem&) = delete;
    MenuItem& operator=(const MenuItem&) = delete;
    MenuItem(MenuItem&&) = delete;
    MenuItem& operator=(MenuItem&&) = delete;

    void setText(std::string text);
    const std::string& getText() const;

    void setDisable(bool value);
    bool isDisable() const;

    void setVisible(bool value);
    bool isVisible() const;

    void setOnAction(ActionHandler handler);
    virtual void fire();

    // mods is a combination of Key::Mod* bits. ModControl matches Ctrl or Command.
    void setAccelerator(int key, int mods);
    int getAcceleratorKey() const;
    int getAcceleratorMods() const;

    // Drawn left of the label. Clicks fall through to the row.
    void setGraphic(std::shared_ptr<Node> graphic);
    std::shared_ptr<Node> getGraphic() const;

    Menu* getParentMenu() const;
    void setParentMenu(Menu* menu);

private:
    friend class Menu;
    friend class MenuBar;

    void notifyParent() const;
    // MenuBar calls this on a top-level Menu it owns directly (not a submenu, which
    // tells its parent Menu instead). Lets the bar lay its titles out again when the
    // menu's text, enabled state, or visibility changes under it.
    void setChangeNotifier(std::function<void()> notifier) { onChanged_ = std::move(notifier); }

    std::string text_;
    bool disable_ = false;
    bool visible_ = true;
    ActionHandler onAction_;
    int acceleratorKey_ = 0;
    int acceleratorMods_ = 0;
    std::shared_ptr<Node> graphic_;
    Menu* parent_ = nullptr;
    std::function<void()> onChanged_;
};

// A non-interactive divider. It is not activated by a click.
class SeparatorMenuItem : public MenuItem {
public:
    SeparatorMenuItem();
};

}  // namespace jadefx
