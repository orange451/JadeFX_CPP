#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/scene/controls/MenuItem.hpp"

#include <memory>
#include <string>

namespace jadefx {

class Node;
class Scene;

// A submenu. show() opens its popup on an anchor's edge. The anchor becomes
// the popup owner, so a submenu whose anchor sits inside another popup stays
// up with that parent.
class Menu : public MenuItem {
public:
    Menu();
    explicit Menu(std::string text);
    ~Menu() override;

    ObservableList<std::shared_ptr<MenuItem>>& getItems();
    const ObservableList<std::shared_ptr<MenuItem>>& getItems() const;

    bool isShowing() const;
    void hide();
    void show(Scene& scene, Node* anchor, Side side);

private:
    friend class MenuItem;

    void adopt(std::shared_ptr<MenuItem> item, std::size_t index);
    void release(std::shared_ptr<MenuItem> item, std::size_t index);
    void removeItem(MenuItem* item);
    void refreshIfOpen();
    void rebuildRows();
    void ensurePopup();
    bool contains(const MenuItem* item) const;
    bool reaches(const Menu* target) const;
    bool wouldCycle(const MenuItem* item) const;

    ObservableList<std::shared_ptr<MenuItem>> items_;
    std::shared_ptr<Node> popup_;
    Node* anchor_ = nullptr;
    Side side_ = Side::Bottom;
    bool mute_ = false;
    bool inShow_ = false;
    bool inHide_ = false;
};

// First visible, enabled, non-separator item whose accelerator matches.
// Menus are not activated; their children are searched.
MenuItem* matchMenuAccelerator(const ObservableList<std::shared_ptr<MenuItem>>& items, const KeyEvent& event);

}  // namespace jadefx
