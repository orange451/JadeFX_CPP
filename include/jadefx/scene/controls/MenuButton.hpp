#pragma once

#include "jadefx/scene/controls/ButtonBase.hpp"
#include "jadefx/scene/controls/Menu.hpp"

#include <string>

namespace jadefx {

class Scene;

// A button that opens a menu under itself. The open click is not a button action.
class MenuButton : public ButtonBase {
public:
    MenuButton();
    explicit MenuButton(std::string text);
    ~MenuButton() override;

    const char* getElementType() const override { return "menubutton"; }

    ObservableList<std::shared_ptr<MenuItem>>& getItems();

    void show();
    void hide();
    bool isShowing() const;

protected:
    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseReleased(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void sceneChanged(Scene* previous) override;

private:
    void dispatchAccelerator(KeyEvent& event);
    void releaseHook();

    Menu menu_;
    int hookId_ = 0;
    Scene* hookedScene_ = nullptr;
};

}  // namespace jadefx
