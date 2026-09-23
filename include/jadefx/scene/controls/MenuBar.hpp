#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/Menu.hpp"

#include <memory>
#include <vector>

namespace jadefx {

class MenuTitle;
class Scene;

// A horizontal strip of menus. Click a title to open it. While one menu is
// open, moving onto another title switches to that menu.
class MenuBar : public Controls {
public:
    MenuBar();
    ~MenuBar() override;

    const char* getElementType() const override { return "menubar"; }

    ObservableList<std::shared_ptr<Menu>>& getMenus();
    const ObservableList<std::shared_ptr<Menu>>& getMenus() const;

protected:
    void layoutChildren() override;
    void sceneChanged(Scene* previous) override;
    void handleMouseMoved(const MouseEvent& event) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    friend class MenuTitle;

    void adoptMenu(std::shared_ptr<Menu> menu, std::size_t index);
    void releaseMenu(std::shared_ptr<Menu> menu, std::size_t index);
    void rebuildTitles();
    void openMenu(Menu* menu, Node* title);
    void hoverMenu(Menu* menu, Node* title);
    bool consumeHoverOpen(Menu* menu);
    void hideMenus();
    void dispatchAccelerator(KeyEvent& event);
    void releaseHook();
    bool anyMenuShowing() const;

    ObservableList<std::shared_ptr<Menu>> menus_;
    std::vector<std::shared_ptr<Node>> titles_;
    // noteButton moves before it presses. The move switches menus, and the
    // press would otherwise see the new menu as already open and close it.
    Menu* suppressToggle_ = nullptr;
    int hookId_ = 0;
    Scene* hookedScene_ = nullptr;
    bool mute_ = false;
};

}  // namespace jadefx
