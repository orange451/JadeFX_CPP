#include "jadefx/scene/Controls/Menu.hpp"

#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/Controls/Label.hpp"
#include "jadefx/scene/layout/VBox.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace jadefx {
namespace {

constexpr double kPopupPad = 4;
constexpr double kRowPadX = 12;
constexpr double kRowHeight = 28;
constexpr double kSeparatorHeight = 9;
constexpr double kMinPopupWidth = 160;
constexpr double kLabelGap = 16;

Color MenuGray() { return Color::rgb8(95, 99, 104); }

Color MenuHover() { return Color::rgb8(232, 240, 254); }

Color MenuLine() { return Color::rgb8(218, 220, 224); }

std::string KeyName(int key) {
    if (key >= Key::A && key <= Key::Z) {
        return std::string(1, static_cast<char>(key));
    }
    if (key >= Key::Digit0 && key <= Key::Digit9) {
        return std::string(1, static_cast<char>(key));
    }
    switch (key) {
        case Key::Space:
            return "Space";
        case Key::Enter:
        case Key::KpEnter:
            return "Enter";
        case Key::Tab:
            return "Tab";
        case Key::Escape:
            return "Esc";
        case Key::Backspace:
            return "Backspace";
        case Key::Delete:
            return "Delete";
        case Key::Left:
            return "Left";
        case Key::Right:
            return "Right";
        case Key::Up:
            return "Up";
        case Key::Down:
            return "Down";
        case Key::Home:
            return "Home";
        case Key::End:
            return "End";
        case Key::PageUp:
            return "PageUp";
        case Key::PageDown:
            return "PageDown";
        default:
            return {};
    }
}

std::string AcceleratorText(const MenuItem& item) {
    if (item.getAcceleratorKey() == 0) {
        return {};
    }
    const std::string name = KeyName(item.getAcceleratorKey());
    if (name.empty()) {
        return {};
    }
    std::string text;
    const int mods = item.getAcceleratorMods();
    if ((mods & (Key::ModControl | Key::ModSuper)) != 0) {
        text += (mods & Key::ModControl) == 0 ? "Cmd+" : "Ctrl+";
    }
    if ((mods & Key::ModAlt) != 0) {
        text += "Alt+";
    }
    if ((mods & Key::ModShift) != 0) {
        text += "Shift+";
    }
    text += name;
    return text;
}

Font MeasureFont(const Node* anchor) {
    if (anchor != nullptr && anchor->computedStyle().fontSize > 0.f) {
        const ComputedStyle& style = anchor->computedStyle();
        const std::string family = style.fontFamily.empty() ? std::string("Open Sans") : style.fontFamily;
        return Font(family, style.fontSize);
    }
    return Font("Open Sans", 16.f);
}

bool AcceleratorMatches(const MenuItem& item, const KeyEvent& event) {
    const int key = item.getAcceleratorKey();
    if (key == 0 || event.key != key) {
        return false;
    }
    const int mods = item.getAcceleratorMods();
    const bool wantShift = (mods & Key::ModShift) != 0;
    const bool wantAlt = (mods & Key::ModAlt) != 0;
    const bool wantShortcut = (mods & (Key::ModControl | Key::ModSuper)) != 0;
    return event.shift == wantShift && event.alt == wantAlt && event.shortcut() == wantShortcut;
}

class MenuPopup : public VBox {
public:
    MenuPopup() {
        // StackPane centers its children. Menu rows pack from the top left.
        setAlignment(Pos::TopLeft);
        setPadding(Insets::uniform(kPopupPad));
        setBackground(Color::white());
        getClassList().add("menu-popup");
        setPrefWidth(kMinPopupWidth);
    }

    const char* getElementType() const override { return "menu-popup"; }

    void render(UiRenderer& renderer, float opacity) override {
        Node::render(renderer, opacity);
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 0.f || height <= 0.f) {
            return;
        }
        const float radius[4] = {4.f, 4.f, 4.f, 4.f};
        const float sides[4] = {1.f, 1.f, 1.f, 1.f};
        Color line = MenuLine();
        line.a *= opacity;
        renderer.strokeRounded(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()), width, height,
                               radius, sides, line);
    }
};

class MenuSeparatorRow : public Region {
public:
    MenuSeparatorRow() {
        setPrefHeight(kSeparatorHeight);
        setMouseTransparent(true);
        getClassList().add("menu-separator");
    }

    const char* getElementType() const override { return "separator"; }

    void renderContent(UiRenderer& renderer, float opacity) override {
        const float width = static_cast<float>(getWidth());
        if (width <= 16.f) {
            return;
        }
        const float radius[4] = {0.f, 0.f, 0.f, 0.f};
        const float at = 0.f;
        Color color = MenuLine();
        color.a *= opacity;
        const float y = static_cast<float>(getAbsoluteY() + getHeight() * 0.5);
        renderer.fillRounded(static_cast<float>(getAbsoluteX()) + 8.f, y, width - 16.f, 1.f, radius, &color, &at, 1,
                             0.f);
    }
};

Menu* RootMenu(MenuItem* item) {
    Menu* menu = item == nullptr ? nullptr : item->getParentMenu();
    while (menu != nullptr && menu->getParentMenu() != nullptr) {
        menu = menu->getParentMenu();
    }
    return menu;
}

void HoverRow(MenuItem* item, Node* row);

class MenuRow : public Region {
public:
    explicit MenuRow(std::shared_ptr<MenuItem> item) : item_(std::move(item)) {
        setPrefHeight(kRowHeight);
        setPadding(Insets::axes(0, kRowPadX));
        getClassList().add("menu-item");
        if (item_ && !item_->getText().empty()) {
            setElementId(item_->getText());
        }
        if (!item_) {
            return;
        }
        text_ = std::make_shared<Label>(item_->getText());
        text_->setMouseTransparent(true);
        text_->setAlignment(Pos::CenterLeft);
        children().add(text_);

        const std::string accel = AcceleratorText(*item_);
        if (!accel.empty()) {
            accel_ = std::make_shared<Label>(accel);
            accel_->setMouseTransparent(true);
            accel_->setTextFill(MenuGray());
            accel_->setAlignment(Pos::CenterRight);
            children().add(accel_);
        }
        if (dynamic_cast<Menu*>(item_.get()) != nullptr) {
            arrow_ = std::make_shared<Label>(">");
            arrow_->setMouseTransparent(true);
            arrow_->setTextFill(MenuGray());
            arrow_->setAlignment(Pos::CenterRight);
            children().add(arrow_);
        }
        if (item_->isDisable()) {
            setDisable(true);
            setOpacity(0.45f);
        }
    }

    const char* getElementType() const override { return "menu-item"; }

    void handleMousePressed(const MouseEvent&) override {
        const std::shared_ptr<MenuItem> item = item_;
        if (!item || item->isDisable() || !item->isVisible()) {
            return;
        }
        if (auto* sub = dynamic_cast<Menu*>(item.get())) {
            if (getScene() != nullptr) {
                sub->show(*getScene(), this, Side::Right);
            }
            return;
        }
        Menu* root = RootMenu(item.get());
        item->fire();
        if (root != nullptr) {
            root->hide();
        }
    }

    void handleMouseMoved(const MouseEvent&) override {
        if (!item_ || item_->isDisable()) {
            return;
        }
        HoverRow(item_.get(), this);
    }

    void render(UiRenderer& renderer, float opacity) override {
        if (isHovered() && !isDisabled()) {
            const float radius[4] = {2.f, 2.f, 2.f, 2.f};
            const float at = 0.f;
            Color wash = MenuHover();
            wash.a *= opacity;
            renderer.fillRounded(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                                 static_cast<float>(getWidth()), static_cast<float>(getHeight()), radius, &wash, &at, 1,
                                 0.f);
        }
        Node::render(renderer, opacity);
    }

protected:
    void layoutChildren() override {
        const double top = contentTop();
        const double left = contentLeft();
        const double innerWidth = contentWidth();
        const double innerHeight = contentHeight();
        double right = left + innerWidth;
        if (arrow_) {
            const double width = arrow_->measuredWidth(innerWidth);
            const double height = std::max(1.0, arrow_->measuredHeight(width, innerHeight));
            right -= width;
            arrow_->performLayout(right, top + (innerHeight - height) * 0.5, width, height);
            right -= 8;
        }
        if (accel_) {
            const double width = accel_->measuredWidth(innerWidth);
            const double height = std::max(1.0, accel_->measuredHeight(width, innerHeight));
            right -= width;
            accel_->performLayout(right, top + (innerHeight - height) * 0.5, width, height);
        }
        if (text_) {
            const double available = std::max(0.0, right - left - (accel_ || arrow_ ? 8.0 : 0.0));
            double width = text_->measuredWidth(available);
            if (width > available) {
                width = available;
            }
            const double height = std::max(1.0, text_->measuredHeight(width, innerHeight));
            text_->performLayout(left, top + (innerHeight - height) * 0.5, width, height);
        }
    }

private:
    std::shared_ptr<MenuItem> item_;
    std::shared_ptr<Label> text_;
    std::shared_ptr<Label> accel_;
    std::shared_ptr<Label> arrow_;
};

void HoverRow(MenuItem* item, Node* row) {
    Menu* parent = item == nullptr ? nullptr : item->getParentMenu();
    if (parent == nullptr || row == nullptr || row->getScene() == nullptr) {
        return;
    }
    for (const std::shared_ptr<MenuItem>& entry : parent->getItems().items()) {
        auto* sub = dynamic_cast<Menu*>(entry.get());
        if (sub == nullptr) {
            continue;
        }
        if (entry.get() == item) {
            if (!sub->isDisable() && sub->isVisible() && !sub->isShowing()) {
                sub->show(*row->getScene(), row, Side::Right);
            }
        } else if (sub->isShowing()) {
            sub->hide();
        }
    }
}

double RowContentWidth(const MenuItem& item, const Font& font) {
    double width = kRowPadX * 2 + static_cast<double>(font.measureWidth(item.getText()));
    const std::string accel = AcceleratorText(item);
    if (!accel.empty()) {
        width += kLabelGap + static_cast<double>(font.measureWidth(accel));
    }
    if (dynamic_cast<const Menu*>(&item) != nullptr) {
        width += 8.0 + static_cast<double>(font.measureWidth(">"));
    }
    return width;
}

}  // namespace

Menu::Menu() : Menu(std::string()) {}

Menu::Menu(std::string text) : MenuItem(std::move(text)) {
    items_.setIndexedAddCallback([this](std::shared_ptr<MenuItem> item, std::size_t index) {
        adopt(std::move(item), index);
    });
    items_.setIndexedRemoveCallback([this](std::shared_ptr<MenuItem> item, std::size_t index) {
        release(std::move(item), index);
    });
}

Menu::~Menu() {
    mute_ = true;
    items_.setIndexedAddCallback(nullptr);
    items_.setIndexedRemoveCallback(nullptr);
    hide();
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (entry && entry->getParentMenu() == this) {
            entry->setParentMenu(nullptr);
        }
    }
}

ObservableList<std::shared_ptr<MenuItem>>& Menu::getItems() { return items_; }

const ObservableList<std::shared_ptr<MenuItem>>& Menu::getItems() const { return items_; }

bool Menu::isShowing() const {
    return popup_ != nullptr && popup_->getScene() != nullptr && popup_->getScene()->isPopupShowing(popup_.get());
}

void Menu::hide() {
    if (inHide_) {
        return;
    }
    if (popup_ != nullptr && popup_->getScene() != nullptr && popup_->getScene()->isTearingDown()) {
        anchor_ = nullptr;
        return;
    }
    inHide_ = true;
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (auto* sub = dynamic_cast<Menu*>(entry.get())) {
            sub->hide();
        }
    }
    if (popup_ != nullptr && popup_->getScene() != nullptr) {
        popup_->getScene()->hidePopup(popup_.get());
    }
    anchor_ = nullptr;
    inHide_ = false;
}

void Menu::show(Scene& scene, Node* anchor, Side side) {
    if (anchor == nullptr || inShow_) {
        return;
    }
    inShow_ = true;
    anchor_ = anchor;
    side_ = side;
    rebuildRows();
    PopupOptions options;
    options.owner = anchor;
    options.autoHide = true;
    scene.showPopupNear(popup_, anchor, side, options);
    inShow_ = false;
}

void Menu::adopt(std::shared_ptr<MenuItem> item, std::size_t index) {
    if (mute_) {
        return;
    }
    if (!item || wouldCycle(item.get())) {
        mute_ = true;
        items_.removeAt(index);
        mute_ = false;
        return;
    }
    std::size_t copies = 0;
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (entry.get() == item.get()) {
            ++copies;
        }
    }
    if (copies > 1) {
        mute_ = true;
        items_.removeAt(index);
        mute_ = false;
        return;
    }
    if (item->getParentMenu() != nullptr && item->getParentMenu() != this) {
        item->getParentMenu()->removeItem(item.get());
    }
    item->setParentMenu(this);
    refreshIfOpen();
}

void Menu::release(std::shared_ptr<MenuItem> item, std::size_t) {
    if (mute_ || !item) {
        return;
    }
    if (item->getParentMenu() == this && !contains(item.get())) {
        item->setParentMenu(nullptr);
        if (auto* sub = dynamic_cast<Menu*>(item.get())) {
            sub->hide();
        }
    }
    refreshIfOpen();
}

void Menu::removeItem(MenuItem* item) {
    items_.removeIf([item](const std::shared_ptr<MenuItem>& entry) { return entry.get() == item; });
}

void Menu::refreshIfOpen() {
    if (mute_ || inShow_ || inHide_ || !isShowing() || anchor_ == nullptr || popup_ == nullptr) {
        return;
    }
    Scene* scene = popup_->getScene();
    if (scene == nullptr) {
        return;
    }
    show(*scene, anchor_, side_);
}

void Menu::ensurePopup() {
    if (popup_) {
        return;
    }
    popup_ = std::make_shared<MenuPopup>();
}

void Menu::rebuildRows() {
    ensurePopup();
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (auto* sub = dynamic_cast<Menu*>(entry.get())) {
            sub->hide();
        }
    }
    auto* popup = static_cast<MenuPopup*>(popup_.get());
    const Font font = MeasureFont(anchor_);
    double widest = 0;
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (!entry || !entry->isVisible() || dynamic_cast<SeparatorMenuItem*>(entry.get()) != nullptr) {
            continue;
        }
        widest = std::max(widest, RowContentWidth(*entry, font));
    }
    const double popupWidth = std::max(kMinPopupWidth, widest + kPopupPad * 2);
    const double rowWidth = std::max(0.0, popupWidth - kPopupPad * 2);
    popup->setPrefWidth(popupWidth);
    popup->getChildren().clear();
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (!entry || !entry->isVisible()) {
            continue;
        }
        if (dynamic_cast<SeparatorMenuItem*>(entry.get()) != nullptr) {
            auto row = std::make_shared<MenuSeparatorRow>();
            row->setPrefWidth(rowWidth);
            popup->getChildren().add(std::move(row));
            continue;
        }
        auto row = std::make_shared<MenuRow>(entry);
        row->setPrefWidth(rowWidth);
        popup->getChildren().add(std::move(row));
    }
}

bool Menu::contains(const MenuItem* item) const {
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        if (entry.get() == item) {
            return true;
        }
    }
    return false;
}

bool Menu::reaches(const Menu* target) const {
    if (target == nullptr) {
        return false;
    }
    if (this == target) {
        return true;
    }
    for (const std::shared_ptr<MenuItem>& entry : items_.items()) {
        const auto* menu = dynamic_cast<const Menu*>(entry.get());
        if (menu != nullptr && menu->reaches(target)) {
            return true;
        }
    }
    return false;
}

bool Menu::wouldCycle(const MenuItem* item) const {
    if (item == nullptr || item == this) {
        return true;
    }
    const auto* menu = dynamic_cast<const Menu*>(item);
    return menu != nullptr && menu->reaches(this);
}

MenuItem* matchMenuAccelerator(const ObservableList<std::shared_ptr<MenuItem>>& items, const KeyEvent& event) {
    for (const std::shared_ptr<MenuItem>& entry : items.items()) {
        if (!entry || !entry->isVisible() || entry->isDisable()) {
            continue;
        }
        if (dynamic_cast<SeparatorMenuItem*>(entry.get()) != nullptr) {
            continue;
        }
        if (auto* menu = dynamic_cast<Menu*>(entry.get())) {
            if (MenuItem* nested = matchMenuAccelerator(menu->getItems(), event)) {
                return nested;
            }
            continue;
        }
        if (AcceleratorMatches(*entry, event)) {
            return entry.get();
        }
    }
    return nullptr;
}

}  // namespace jadefx
