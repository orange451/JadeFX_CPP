#include "jadefx/scene/Controls/Alert.hpp"

#include "jadefx/event/Events.hpp"
#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/Controls/Button.hpp"
#include "jadefx/scene/Controls/Label.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/layout/HBox.hpp"
#include "jadefx/scene/layout/StackPane.hpp"
#include "jadefx/scene/layout/VBox.hpp"
#include "jadefx/scene/text/Font.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace jadefx {
namespace {

class AlertPanel : public VBox {
public:
    const char* getElementType() const override { return "alert"; }
};

std::string ChromeText(AlertType type) {
    switch (type) {
        case AlertType::Information:
            return "Information";
        case AlertType::Warning:
            return "Warning";
        case AlertType::Confirmation:
            return "Confirmation";
        case AlertType::Error:
            return "Error";
        case AlertType::None:
            return {};
    }
    return {};
}

Color TitleColor(AlertType type) {
    switch (type) {
        case AlertType::Information:
            return Color::rgb8(0x1a, 0x73, 0xe8);
        case AlertType::Warning:
            return Color::rgb8(0xe3, 0x74, 0x00);
        case AlertType::Error:
            return Color::rgb8(0xd9, 0x30, 0x25);
        case AlertType::Confirmation:
            return Color::rgb8(0x18, 0x80, 0x38);
        case AlertType::None:
            return Color::rgb8(0x20, 0x21, 0x24);
    }
    return Color::rgb8(0x20, 0x21, 0x24);
}

// CancelClose, Other, Left, Right, OkDone. Equal roles keep their list order.
int ButtonRank(ButtonType::Data data) {
    switch (data) {
        case ButtonType::Data::CancelClose:
            return 0;
        case ButtonType::Data::Other:
            return 1;
        case ButtonType::Data::Left:
            return 2;
        case ButtonType::Data::Right:
            return 3;
        case ButtonType::Data::OkDone:
            return 4;
    }
    return 1;
}

std::vector<ButtonType> DefaultButtons(AlertType type) {
    switch (type) {
        case AlertType::Information:
        case AlertType::Warning:
        case AlertType::Error:
            return {ButtonType::Ok()};
        case AlertType::Confirmation:
            return {ButtonType::Ok(), ButtonType::Cancel()};
        case AlertType::None:
            return {};
    }
    return {};
}

}  // namespace

struct Alert::Impl {
    struct Built {
        ButtonType type;
        std::shared_ptr<Button> node;
    };

    AlertType type = AlertType::None;
    // fallback* is the type's word. An explicit set, even to "", replaces it for good.
    std::string fallbackTitle;
    std::string fallbackHeader;
    std::string title;
    std::string header;
    std::string content;
    bool titleSet = false;
    bool headerSet = false;
    ObservableList<ButtonType> buttons;
    std::optional<ButtonType> result;
    std::function<void(const ButtonType*)> onClosed;

    std::vector<Built> built;
    std::shared_ptr<StackPane> dimmer;
    std::shared_ptr<AlertPanel> panel;
    Button* defaultButton = nullptr;
    Button* cancelButton = nullptr;
    Scene* scene = nullptr;
    int hookId = 0;
    bool hookLive = false;

    const std::string& shownTitle() const { return titleSet ? title : fallbackTitle; }
    const std::string& shownHeader() const { return headerSet ? header : fallbackHeader; }

    void dismiss() {
        // The dimmer's scene pointer is cleared while the scene is still alive.
        // A stored Scene* would dangle after that.
        Scene* live = dimmer ? dimmer->getScene() : nullptr;
        if (live == nullptr || live->isTearingDown()) {
            hookLive = false;
            hookId = 0;
            scene = nullptr;
            return;
        }
        scene = live;
        if (hookLive) {
            scene->removeKeyHook(hookId);
        }
        hookLive = false;
        hookId = 0;
        if (scene != nullptr && dimmer != nullptr && scene->isPopupShowing(dimmer.get())) {
            scene->hidePopup(dimmer.get());
        }
        scene = nullptr;
    }
};

Alert::Alert(AlertType type) : Alert(type, std::string(), {}) {}

Alert::Alert(AlertType type, std::string contentText, std::vector<ButtonType> buttons) : impl_(std::make_unique<Impl>()) {
    impl_->type = type;
    impl_->fallbackTitle = ChromeText(type);
    impl_->fallbackHeader = ChromeText(type);
    impl_->content = std::move(contentText);
    if (buttons.empty()) {
        buttons = DefaultButtons(type);
    }
    for (ButtonType& button : buttons) {
        impl_->buttons.add(std::move(button));
    }
}

Alert::~Alert() { impl_->dismiss(); }

void Alert::setTitle(std::string title) {
    impl_->titleSet = true;
    impl_->title = std::move(title);
}

const std::string& Alert::getTitle() const { return impl_->shownTitle(); }

void Alert::setHeaderText(std::string text) {
    impl_->headerSet = true;
    impl_->header = std::move(text);
}

const std::string& Alert::getHeaderText() const { return impl_->shownHeader(); }

void Alert::setContentText(std::string text) { impl_->content = std::move(text); }

const std::string& Alert::getContentText() const { return impl_->content; }

AlertType Alert::getAlertType() const { return impl_->type; }

ObservableList<ButtonType>& Alert::getButtonTypes() { return impl_->buttons; }

Button* Alert::lookupButton(const ButtonType& type) const {
    for (const Impl::Built& built : impl_->built) {
        if (built.node && built.type == type) {
            return built.node.get();
        }
    }
    return nullptr;
}

void Alert::setOnClosed(std::function<void(const ButtonType*)> handler) { impl_->onClosed = std::move(handler); }

const ButtonType* Alert::getResult() const { return impl_->result ? &*impl_->result : nullptr; }

void Alert::setResult(const ButtonType& type) {
    impl_->result = type;
    const std::function<void(const ButtonType*)> handler = impl_->onClosed;
    impl_->dismiss();
    if (handler) {
        handler(getResult());
    }
}

void Alert::show(Scene& scene) {
    impl_->result.reset();
    impl_->dismiss();
    impl_->defaultButton = nullptr;
    impl_->cancelButton = nullptr;
    impl_->built.clear();
    impl_->panel.reset();
    impl_->dimmer.reset();
    impl_->scene = &scene;

    auto dimmer = std::make_shared<StackPane>();
    dimmer->setBackground(Color::rgba(0.f, 0.f, 0.f, 0.35f));

    auto panel = std::make_shared<AlertPanel>();
    panel->setPrefWidth(420);
    panel->setPadding(Insets::uniform(20));
    panel->setSpacing(8);
    panel->setBackground(Color::white());
    panel->setStyle("border-radius: 8px; box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.3)");

    const std::string& title = impl_->shownTitle();
    if (!title.empty()) {
        auto label = std::make_shared<Label>(title);
        label->setFont(Font("Open Sans", 18.f));
        label->setTextFill(TitleColor(impl_->type));
        panel->getChildren().add(label);
    }
    const std::string& header = impl_->shownHeader();
    if (!header.empty()) {
        panel->getChildren().add(std::make_shared<Label>(header));
    }
    if (!impl_->content.empty()) {
        auto label = std::make_shared<Label>(impl_->content);
        label->setPrefWidth(380);
        panel->getChildren().add(label);
    }

    std::vector<std::size_t> order(impl_->buttons.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return ButtonRank(impl_->buttons[a].getButtonData()) < ButtonRank(impl_->buttons[b].getButtonData());
    });

    auto row = std::make_shared<HBox>();
    row->setSpacing(8);
    row->setAlignment(Pos::CenterRight);
    for (std::size_t index : order) {
        const ButtonType type = impl_->buttons[index];
        auto button = std::make_shared<Button>(type.getText());
        if (type.getButtonData() == ButtonType::Data::OkDone) {
            button->setDefaultButton(true);
            if (impl_->defaultButton == nullptr) {
                impl_->defaultButton = button.get();
            }
        } else if (type.getButtonData() == ButtonType::Data::CancelClose) {
            button->setCancelButton(true);
            if (impl_->cancelButton == nullptr) {
                impl_->cancelButton = button.get();
            }
        }
        button->setOnAction([this, type](ActionEvent&) { setResult(type); });
        impl_->built.push_back(Impl::Built{type, button});
        row->getChildren().add(button);
    }

    if (!impl_->built.empty()) {
        auto bar = std::make_shared<StackPane>();
        bar->setAlignment(Pos::CenterRight);
        bar->setPrefWidth(380);
        bar->getChildren().add(row);
        panel->getChildren().add(bar);
    }

    dimmer->getChildren().add(panel);
    impl_->dimmer = dimmer;
    impl_->panel = panel;

    PopupOptions options;
    options.modal = true;
    options.autoHide = false;
    options.hideOnPress = false;
    options.fillScene = true;
    scene.showPopup(impl_->dimmer, 0, 0, -1, -1, options);

    // Buttons, not the alert: fire() may drop the alert before the key is consumed.
    Button* const accept = impl_->defaultButton;
    Button* const cancel = impl_->cancelButton;
    impl_->hookId = scene.addKeyHook([accept, cancel](KeyEvent& event) {
        if (!event.pressed || event.repeat) {
            return;
        }
        Button* target = nullptr;
        if (event.key == Key::Enter || event.key == Key::KpEnter) {
            target = accept;
        } else if (event.key == Key::Escape) {
            target = cancel;
        }
        if (target == nullptr) {
            return;
        }
        target->fire();
        event.consume();
    });
    impl_->hookLive = true;

    if (impl_->defaultButton != nullptr) {
        impl_->defaultButton->requestFocus();
    } else if (!impl_->built.empty() && impl_->built.front().node) {
        impl_->built.front().node->requestFocus();
    }
}

const ButtonType* Alert::showAndWait(Scene& scene) {
    show(scene);
    while (getResult() == nullptr) {
        const int turn = scene.runEventPump();
        if (turn != 1) {
            break;
        }
    }
    return getResult();
}

}  // namespace jadefx
