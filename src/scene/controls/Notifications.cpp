#include "jadefx/scene/controls/Notifications.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/controls/Button.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/layout/HBox.hpp"
#include "jadefx/scene/layout/VBox.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jadefx {
namespace {

constexpr double kMargin = 16;
constexpr double kGap = 8;
constexpr double kFadeIn = 0.18;
constexpr double kFadeOut = 0.25;
// How far a card slides in from, toward its edge.
constexpr double kSlide = 12;
// How long a card stays after the pointer leaves it, when its time ran out under the pointer.
constexpr double kLinger = 1.5;
// How quickly cards slide to their places in the stack, per second.
constexpr double kSettle = 14;
constexpr const char* kCenterKey = "jadefx.notifications";

// The round mark of a notification's type, drawn in the theme's color for it.
class TypeIcon : public Controls {
public:
    explicit TypeIcon(AlertType type) : type_(type) {
        setPrefSize(24, 24);
        setFocusTraversable(false);
    }

    const char* getElementType() const override { return "notification-icon"; }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        const float size = static_cast<float>(std::min(getWidth(), getHeight()));
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float radius[4] = {size * 0.5f, size * 0.5f, size * 0.5f, size * 0.5f};
        const float at = 0.f;
        const Color fill = chrome::Themed(*this, ColorOf(type_), opacity);
        renderer.fillRounded(x, y, size, size, radius, &fill, &at, 1, 0.f);
        const Font font = chrome::FontOf(*this);
        const std::string mark = MarkOf(type_);
        const float width = font.measureWidth(mark);
        renderer.text(x + (size - width) * 0.5f, y + (size - font.lineHeight()) * 0.5f, mark, font.family(), font.size(),
                      chrome::Themed(*this, ThemeColor::Surface, opacity), computedStyle().subpixel);
    }

private:
    static ThemeColor ColorOf(AlertType type) {
        switch (type) {
            case AlertType::Warning: return ThemeColor::Warning;
            case AlertType::Error: return ThemeColor::Error;
            case AlertType::Confirmation: return ThemeColor::Accent;
            default: return ThemeColor::Info;
        }
    }

    static const char* MarkOf(AlertType type) {
        switch (type) {
            case AlertType::Warning: return "!";
            case AlertType::Error: return "\xC3\x97";  // ×
            case AlertType::Confirmation: return "?";
            default: return "i";
        }
    }

    AlertType type_;
};

const char* TypeClass(AlertType type) {
    switch (type) {
        case AlertType::Information: return "information";
        case AlertType::Warning: return "warning";
        case AlertType::Error: return "error";
        case AlertType::Confirmation: return "confirmation";
        default: return nullptr;
    }
}

int HorizontalOf(Pos pos) {
    switch (pos) {
        case Pos::TopLeft:
        case Pos::CenterLeft:
        case Pos::BottomLeft: return 0;
        case Pos::TopCenter:
        case Pos::Center:
        case Pos::BottomCenter: return 1;
        default: return 2;
    }
}

// -1 stacks down from the top, 1 up from the bottom, 0 down from the middle.
int VerticalOf(Pos pos) {
    switch (pos) {
        case Pos::TopLeft:
        case Pos::TopCenter:
        case Pos::TopRight: return -1;
        case Pos::CenterLeft:
        case Pos::Center:
        case Pos::CenterRight: return 0;
        default: return 1;
    }
}

}  // namespace

// The cards and toasts of one scene: it shows them as popups, stacks them by
// position, fades them in and out, and closes them when their time is up. It
// lives in the scene's properties and runs after each layout while it has cards.
class NotificationCenter {
public:
    explicit NotificationCenter(Scene& scene) : scene_(scene) {}

    static NotificationCenter& of(Scene& scene) {
        std::any& slot = scene.getProperties()[kCenterKey];
        if (!slot.has_value()) {
            slot = std::make_shared<NotificationCenter>(scene);
        }
        return *std::any_cast<std::shared_ptr<NotificationCenter>&>(slot);
    }

    void add(std::shared_ptr<Node> node, Pos position, double seconds) {
        Card card;
        card.node = node;
        card.position = position;
        card.seconds = seconds;
        cards_.push_back(card);
        node->setOpacity(0.f);
        PopupOptions options;
        options.autoHide = false;
        // Placed by the first pulse, once it has been measured.
        scene_.showPopup(node, -10000, -10000, -1, -1, options);
        if (listener_ == 0) {
            listener_ = scene_.addPostLayoutPulseListener([this] { pulse(); });
        }
    }

    void close(const Node* node) {
        for (Card& card : cards_) {
            if (card.node.get() == node && card.closingAt < 0) {
                card.closingAt = scene_.timeSeconds();
            }
        }
    }

    void closeAll() {
        for (Card& card : cards_) {
            close(card.node.get());
        }
    }

private:
    struct Card {
        std::shared_ptr<Node> node;
        Pos position = Pos::BottomRight;
        double seconds = 0;
        double shownAt = -1;
        double hideAt = std::numeric_limits<double>::infinity();
        double closingAt = -1;
        double y = std::numeric_limits<double>::quiet_NaN();
    };

    void pulse() {
        const double now = scene_.timeSeconds();
        const double step = lastPulse_ < 0 ? 1.0 : std::clamp((now - lastPulse_) * kSettle, 0.0, 1.0);
        lastPulse_ = now;
        for (Card& card : cards_) {
            if (card.shownAt < 0) {
                card.shownAt = now;
                if (card.seconds > 0) {
                    card.hideAt = now + card.seconds;
                }
            }
            // A pointer on the card holds it, so it can be read and clicked.
            if (card.node->isHovered() && std::isfinite(card.hideAt)) {
                card.hideAt = std::max(card.hideAt, now + kLinger);
            }
            if (card.closingAt < 0 && now >= card.hideAt) {
                card.closingAt = now;
            }
        }
        // Gone cards leave first, so the rest settle into the room.
        cards_.erase(std::remove_if(cards_.begin(), cards_.end(),
                                    [&](const Card& card) {
                                        const bool gone = card.closingAt >= 0 && now - card.closingAt >= kFadeOut;
                                        if (gone) {
                                            scene_.hidePopup(card.node.get());
                                        }
                                        return gone;
                                    }),
                     cards_.end());
        placeCards(now, step);
        if (cards_.empty()) {
            scene_.removePostLayoutPulseListener(listener_);
            listener_ = 0;
            lastPulse_ = -1;
        }
    }

    // Stacks each position's cards from its edge, newest nearest the edge.
    void placeCards(double now, double step) {
        const double width = scene_.getWidth();
        const double height = scene_.getHeight();
        std::vector<double> offsets;
        std::vector<Pos> positions;
        for (auto it = cards_.rbegin(); it != cards_.rend(); ++it) {
            Card& card = *it;
            const auto found = std::find(positions.begin(), positions.end(), card.position);
            const std::size_t slot = static_cast<std::size_t>(found - positions.begin());
            if (found == positions.end()) {
                positions.push_back(card.position);
                offsets.push_back(0.0);
            }
            const double w = card.node->getWidth();
            const double h = card.node->getHeight();
            const int horizontal = HorizontalOf(card.position);
            const int vertical = VerticalOf(card.position);
            const double x = horizontal == 0 ? kMargin : horizontal == 1 ? (width - w) * 0.5 : width - w - kMargin;
            const double offset = offsets[slot];
            double target = vertical < 0 ? kMargin + offset
                            : vertical > 0 ? height - kMargin - offset - h
                                           : height * 0.5 + offset;
            // Cards that are leaving keep their room until they are gone.
            offsets[slot] += h + kGap;

            const double fadeIn = std::clamp((now - card.shownAt) / kFadeIn, 0.0, 1.0);
            const double fadeOut = card.closingAt < 0 ? 1.0 : 1.0 - std::clamp((now - card.closingAt) / kFadeOut, 0.0, 1.0);
            card.node->setOpacity(static_cast<float>(fadeIn * fadeOut));
            // In from its edge: up from below at the bottom, down from above at the top.
            target += (1.0 - fadeIn) * kSlide * (vertical < 0 ? -1.0 : 1.0);
            card.y = std::isnan(card.y) ? target : card.y + (target - card.y) * step;
            scene_.movePopup(card.node.get(), x, card.y, -1, -1);
        }
    }

    Scene& scene_;
    std::vector<Card> cards_;
    int listener_ = 0;
    double lastPulse_ = -1;
};

Notifications& Notifications::title(std::string text) {
    title_ = std::move(text);
    return *this;
}

Notifications& Notifications::text(std::string text) {
    text_ = std::move(text);
    return *this;
}

Notifications& Notifications::graphic(std::shared_ptr<Node> graphic) {
    graphic_ = std::move(graphic);
    return *this;
}

Notifications& Notifications::position(Pos position) {
    position_ = position;
    return *this;
}

Notifications& Notifications::hideAfter(double seconds) {
    hideAfter_ = seconds;
    return *this;
}

Notifications& Notifications::onAction(ActionHandler handler) {
    onAction_ = std::move(handler);
    return *this;
}

Notifications& Notifications::action(std::string label, ActionHandler handler) {
    actions_.emplace_back(std::move(label), std::move(handler));
    return *this;
}

Notifications& Notifications::hideCloseButton() {
    closeButton_ = false;
    return *this;
}

Notifications& Notifications::styleClass(std::string name) {
    styleClasses_.push_back(std::move(name));
    return *this;
}

Notifications& Notifications::owner(Node& node) {
    owner_ = &node;
    return *this;
}

void Notifications::show() { show(AlertType::None); }
void Notifications::showInformation() { show(AlertType::Information); }
void Notifications::showWarning() { show(AlertType::Warning); }
void Notifications::showError() { show(AlertType::Error); }
void Notifications::showConfirm() { show(AlertType::Confirmation); }

void Notifications::show(AlertType type) {
    Scene* scene = owner_ != nullptr ? owner_->getScene() : nullptr;
    if (scene == nullptr) {
        return;
    }
    NotificationCenter& center = NotificationCenter::of(*scene);
    auto card = std::make_shared<HBox>();
    Node* const cardNode = card.get();
    card->getClassList().add("notification");
    if (const char* typeClass = TypeClass(type)) {
        card->getClassList().add(typeClass);
    }
    for (const std::string& name : styleClasses_) {
        card->getClassList().add(name);
    }
    if (graphic_ != nullptr) {
        card->getChildren().add(graphic_);
    } else if (type != AlertType::None) {
        card->getChildren().add(std::make_shared<TypeIcon>(type));
    }

    auto body = std::make_shared<VBox>();
    body->getClassList().add("body");
    if (!title_.empty()) {
        auto title = std::make_shared<Label>(title_);
        title->getClassList().add("title");
        body->getChildren().add(title);
    }
    std::size_t start = 0;
    while (start <= text_.size() && !text_.empty()) {
        const std::size_t end = std::min(text_.find('\n', start), text_.size());
        auto line = std::make_shared<Label>(text_.substr(start, end - start));
        line->getClassList().add("text");
        body->getChildren().add(line);
        start = end + 1;
    }
    // A button's press is not a click on the card.
    auto buttonPressed = std::make_shared<bool>(false);
    if (!actions_.empty()) {
        auto row = std::make_shared<HBox>();
        row->getClassList().add("actions");
        for (const auto& entry : actions_) {
            auto button = std::make_shared<Button>(entry.first);
            ActionHandler handler = entry.second;
            button->setOnAction([&center, cardNode, handler, buttonPressed](ActionEvent& event) {
                *buttonPressed = true;
                if (handler) {
                    handler(event);
                }
                center.close(cardNode);
            });
            row->getChildren().add(button);
        }
        body->getChildren().add(row);
    }
    card->getChildren().add(body);

    if (closeButton_) {
        auto close = std::make_shared<Button>("\xC3\x97");
        close->getClassList().add("close-button");
        close->setOnAction([&center, cardNode, buttonPressed](ActionEvent&) {
            *buttonPressed = true;
            center.close(cardNode);
        });
        card->getChildren().add(close);
    }
    if (onAction_) {
        card->setStyle("cursor: pointer;");
        ActionHandler handler = onAction_;
        card->setOnMouseClicked([&center, cardNode, handler, buttonPressed](const MouseEvent&) {
            if (*buttonPressed) {
                *buttonPressed = false;
                return;
            }
            ActionEvent event;
            event.source = cardNode;
            handler(event);
            center.close(cardNode);
        });
    }
    center.add(card, position_, hideAfter_);
}

void Notifications::hideAll(Node& owner) {
    if (Scene* scene = owner.getScene()) {
        NotificationCenter::of(*scene).closeAll();
    }
}

void Toast::show(Node& owner, std::string text, double seconds, Pos position) {
    Scene* scene = owner.getScene();
    if (scene == nullptr) {
        return;
    }
    auto toast = std::make_shared<Label>(std::move(text));
    toast->getClassList().add("toast");
    toast->setMouseTransparent(true);
    NotificationCenter::of(*scene).add(toast, position, seconds > 0 ? seconds : LENGTH_SHORT);
}

}  // namespace jadefx
