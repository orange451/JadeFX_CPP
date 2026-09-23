#include "jadefx/scene/Controls/MenuBar.hpp"

#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/Controls/Label.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace jadefx {
namespace {

constexpr double kTitleHeight = 28;
constexpr double kTitlePad = 24;

Font BarFont(const Node& node) {
    const ComputedStyle& style = node.computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    const std::string family = style.fontFamily.empty() ? std::string("Open Sans") : style.fontFamily;
    return Font(family, size);
}

}  // namespace

class MenuTitle : public Region {
public:
    MenuTitle(MenuBar* bar, std::shared_ptr<Menu> menu) : bar_(bar), menu_(std::move(menu)) {
        getClassList().add("menu");
        label_ = std::make_shared<Label>(menu_ ? menu_->getText() : std::string());
        label_->setMouseTransparent(true);
        label_->setAlignment(Pos::Center);
        children().add(label_);
        if (menu_ && !menu_->getText().empty()) {
            setElementId(std::string("menu:") + menu_->getText());
        }
    }

    const char* getElementType() const override { return "menu"; }

    Menu* menu() const { return menu_.get(); }

    void handleMousePressed(const MouseEvent&) override {
        if (bar_ == nullptr || !menu_ || menu_->isDisable() || !menu_->isVisible() || bar_->isDisabled()) {
            return;
        }
        if (menu_->isShowing()) {
            if (bar_->consumeHoverOpen(menu_.get())) {
                return;
            }
            menu_->hide();
            return;
        }
        bar_->openMenu(menu_.get(), this);
    }

    void handleMouseMoved(const MouseEvent&) override {
        if (bar_ == nullptr || !menu_ || menu_->isDisable() || !menu_->isVisible()) {
            return;
        }
        bar_->hoverMenu(menu_.get(), this);
    }

    void render(UiRenderer& renderer, float opacity) override {
        if (menu_ && !menu_->isDisable() && (isHovered() || menu_->isShowing())) {
            const float radius[4] = {0.f, 0.f, 0.f, 0.f};
            const float at = 0.f;
            Color wash = menu_->isShowing() ? Color::rgb8(232, 240, 254) : Color::rgb8(232, 234, 237);
            wash.a *= opacity;
            renderer.fillRounded(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                                 static_cast<float>(getWidth()), static_cast<float>(getHeight()), radius, &wash, &at, 1,
                                 0.f);
        }
        const float faded = menu_ && menu_->isDisable() ? opacity * 0.45f : opacity;
        Node::render(renderer, faded);
    }

protected:
    void layoutChildren() override {
        if (!label_) {
            return;
        }
        if (menu_) {
            label_->setText(menu_->getText());
            if (!menu_->getText().empty()) {
                setElementId(std::string("menu:") + menu_->getText());
            }
        }
        const double innerWidth = contentWidth();
        const double innerHeight = contentHeight();
        double width = label_->measuredWidth(innerWidth);
        if (width > innerWidth) {
            width = innerWidth;
        }
        const double height = std::max(1.0, label_->measuredHeight(width, innerHeight));
        const double x = contentLeft() + std::max(0.0, (innerWidth - width) * 0.5);
        const double y = contentTop() + std::max(0.0, (innerHeight - height) * 0.5);
        label_->performLayout(x, y, width, height);
    }

private:
    MenuBar* bar_ = nullptr;
    std::shared_ptr<Menu> menu_;
    std::shared_ptr<Label> label_;
};

MenuBar::MenuBar() {
    setBackground(Color::rgb8(255, 255, 255));
    menus_.setIndexedAddCallback([this](std::shared_ptr<Menu> menu, std::size_t index) {
        adoptMenu(std::move(menu), index);
    });
    menus_.setIndexedRemoveCallback([this](std::shared_ptr<Menu> menu, std::size_t index) {
        releaseMenu(std::move(menu), index);
    });
}

MenuBar::~MenuBar() {
    mute_ = true;
    menus_.setIndexedAddCallback(nullptr);
    menus_.setIndexedRemoveCallback(nullptr);
    if (hookedScene_ != nullptr && hookedScene_->isTearingDown()) {
        hookId_ = 0;
        hookedScene_ = nullptr;
        titles_.clear();
        return;
    }
    releaseHook();
    hideMenus();
    titles_.clear();
    // Clearing children reparents them. Skip that while an ancestor scene is
    // still tearing this bar down; its popups and key hooks are already gone.
    if (getScene() == nullptr) {
        children().clear();
    }
}

ObservableList<std::shared_ptr<Menu>>& MenuBar::getMenus() { return menus_; }

const ObservableList<std::shared_ptr<Menu>>& MenuBar::getMenus() const { return menus_; }

void MenuBar::layoutChildren() {
    const Font font = BarFont(*this);
    double x = contentLeft();
    const double y = contentTop();
    for (const std::shared_ptr<Node>& title : titles_) {
        auto* item = static_cast<MenuTitle*>(title.get());
        Menu* menu = item->menu();
        if (menu == nullptr || !menu->isVisible()) {
            item->setVisible(false);
            item->performLayout(x, y, 0, 0);
            continue;
        }
        item->setVisible(true);
        const double width = static_cast<double>(font.measureWidth(menu->getText())) + kTitlePad;
        item->performLayout(x, y, width, kTitleHeight);
        x += width;
    }
}

void MenuBar::sceneChanged(Scene* previous) {
    if (previous != nullptr && previous->isTearingDown()) {
        hookId_ = 0;
        hookedScene_ = nullptr;
        return;
    }
    if (hookId_ != 0 && hookedScene_ != nullptr && hookedScene_ != getScene()) {
        hookedScene_->removeKeyHook(hookId_);
        hookId_ = 0;
        hookedScene_ = nullptr;
    }
    if (previous != nullptr && getScene() == nullptr) {
        hideMenus();
    }
    if (getScene() != nullptr && hookId_ == 0) {
        hookId_ = getScene()->addKeyHook([this](KeyEvent& event) { dispatchAccelerator(event); });
        hookedScene_ = getScene();
    }
}

void MenuBar::handleMouseMoved(const MouseEvent& event) {
    for (const std::shared_ptr<Node>& title : titles_) {
        if (!title || !title->isVisible() || !title->contains(event.x, event.y)) {
            continue;
        }
        hoverMenu(static_cast<MenuTitle*>(title.get())->menu(), title.get());
        return;
    }
}

double MenuBar::preferredContentWidth(double) const {
    const Font font = BarFont(*this);
    double width = 0;
    for (const std::shared_ptr<Menu>& menu : menus_.items()) {
        if (!menu || !menu->isVisible()) {
            continue;
        }
        width += static_cast<double>(font.measureWidth(menu->getText())) + kTitlePad;
    }
    return width;
}

double MenuBar::preferredContentHeight(double) const { return kTitleHeight; }

void MenuBar::adoptMenu(std::shared_ptr<Menu> menu, std::size_t index) {
    if (mute_) {
        return;
    }
    if (!menu) {
        mute_ = true;
        menus_.removeAt(index);
        mute_ = false;
        return;
    }
    std::size_t copies = 0;
    for (const std::shared_ptr<Menu>& entry : menus_.items()) {
        if (entry.get() == menu.get()) {
            ++copies;
        }
    }
    if (copies > 1) {
        mute_ = true;
        menus_.removeAt(index);
        mute_ = false;
        return;
    }
    rebuildTitles();
}

void MenuBar::releaseMenu(std::shared_ptr<Menu> menu, std::size_t) {
    if (mute_) {
        return;
    }
    if (menu) {
        menu->hide();
    }
    rebuildTitles();
}

void MenuBar::rebuildTitles() {
    hideMenus();
    titles_.clear();
    children().clear();
    for (const std::shared_ptr<Menu>& menu : menus_.items()) {
        if (!menu) {
            continue;
        }
        auto title = std::make_shared<MenuTitle>(this, menu);
        titles_.push_back(title);
        children().add(std::move(title));
    }
}

void MenuBar::openMenu(Menu* menu, Node* title) {
    if (menu == nullptr || title == nullptr || getScene() == nullptr || isDisabled()) {
        return;
    }
    suppressToggle_ = nullptr;
    for (const std::shared_ptr<Menu>& other : menus_.items()) {
        if (other && other.get() != menu) {
            other->hide();
        }
    }
    menu->show(*getScene(), title, Side::Bottom);
}

void MenuBar::hoverMenu(Menu* menu, Node* title) {
    if (menu == nullptr || title == nullptr || getScene() == nullptr || isDisabled()) {
        return;
    }
    if (!anyMenuShowing() || menu->isShowing()) {
        if (menu->isShowing()) {
            suppressToggle_ = nullptr;
        }
        return;
    }
    for (const std::shared_ptr<Menu>& other : menus_.items()) {
        if (other && other.get() != menu) {
            other->hide();
        }
    }
    menu->show(*getScene(), title, Side::Bottom);
    suppressToggle_ = menu;
}

bool MenuBar::consumeHoverOpen(Menu* menu) {
    if (suppressToggle_ != menu) {
        return false;
    }
    suppressToggle_ = nullptr;
    return true;
}

void MenuBar::hideMenus() {
    suppressToggle_ = nullptr;
    for (const std::shared_ptr<Menu>& menu : menus_.items()) {
        if (menu) {
            menu->hide();
        }
    }
}

void MenuBar::dispatchAccelerator(KeyEvent& event) {
    if (event.consumed || !event.pressed || event.repeat || isDisabled() || !isVisible()) {
        return;
    }
    for (const std::shared_ptr<Menu>& menu : menus_.items()) {
        if (!menu || !menu->isVisible() || menu->isDisable()) {
            continue;
        }
        MenuItem* item = matchMenuAccelerator(menu->getItems(), event);
        if (item == nullptr) {
            continue;
        }
        item->fire();
        event.consume();
        hideMenus();
        return;
    }
}

void MenuBar::releaseHook() {
    if (hookId_ != 0 && hookedScene_ != nullptr && getParent() == nullptr) {
        hookedScene_->removeKeyHook(hookId_);
    }
    hookId_ = 0;
    hookedScene_ = nullptr;
}

bool MenuBar::anyMenuShowing() const {
    for (const std::shared_ptr<Menu>& menu : menus_.items()) {
        if (menu && menu->isShowing()) {
            return true;
        }
    }
    return false;
}

}  // namespace jadefx
