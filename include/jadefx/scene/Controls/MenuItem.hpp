#pragma once

#include "jadefx/event/Events.hpp"

#include <string>

namespace jadefx {

class Menu;

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

    Menu* getParentMenu() const;
    void setParentMenu(Menu* menu);

private:
    friend class Menu;

    void notifyParent() const;

    std::string text_;
    bool disable_ = false;
    bool visible_ = true;
    ActionHandler onAction_;
    int acceleratorKey_ = 0;
    int acceleratorMods_ = 0;
    Menu* parent_ = nullptr;
};

// A non-interactive divider. It is not activated by a click.
class SeparatorMenuItem : public MenuItem {
public:
    SeparatorMenuItem();
};

}  // namespace jadefx
