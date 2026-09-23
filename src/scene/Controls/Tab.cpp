#include "jadefx/scene/Controls/Tab.hpp"

#include "jadefx/scene/Controls/TabPane.hpp"
#include "jadefx/scene/Node.hpp"

namespace jadefx {

void Tab::bindStyleList() {
    styleClass_.setAddCallback([this](const std::string&) {
        if (tabPane_ != nullptr) {
            tabPane_->syncAll();
        }
    });
    styleClass_.setRemoveCallback([this](const std::string&) {
        if (tabPane_ != nullptr) {
            tabPane_->syncAll();
        }
    });
}

Tab::Tab() : Tab("", nullptr) {}

Tab::Tab(std::string text) : Tab(std::move(text), nullptr) {}

Tab::Tab(std::string text, std::shared_ptr<Node> content) : text_(std::move(text)) {
    bindStyleList();
    styleClass_.add("tab");
    content_ = std::move(content);
}

Tab::~Tab() {
    styleClass_.setAddCallback(nullptr);
    styleClass_.setRemoveCallback(nullptr);
    onSelectionChanged_ = nullptr;
    onClosed_ = nullptr;
    onCloseRequest_ = nullptr;
    // The pane unhooks a tab before releasing it. This only covers a tab that
    // still owns the showing page while its pane pointer is alive.
    if (content_ != nullptr && tabPane_ != nullptr && content_->getParent() == tabPane_) {
        content_->setParent(nullptr);
    }
    tabPane_ = nullptr;
}

void Tab::setId(std::string id) {
    if (id_ == id) {
        return;
    }
    id_ = std::move(id);
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::setStyle(std::string css) {
    if (style_ == css) {
        return;
    }
    style_ = std::move(css);
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::setText(std::string text) {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::setGraphic(std::shared_ptr<Node> graphic) {
    if (graphic_ == graphic) {
        return;
    }
    graphic_ = std::move(graphic);
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::setContent(std::shared_ptr<Node> content) {
    if (content_ == content) {
        return;
    }
    std::shared_ptr<Node> previous = std::move(content_);
    content_ = std::move(content);
    if (tabPane_ != nullptr) {
        tabPane_->noteContent(*this, previous);
    }
}

void Tab::setClosable(bool closable) {
    if (closable_ == closable) {
        return;
    }
    closable_ = closable;
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::setDisable(bool value) {
    if (disable_ == value) {
        return;
    }
    disable_ = value;
    updateDisabled();
}

void Tab::updateDisabled() {
    const bool value = disable_ || (tabPane_ != nullptr && tabPane_->isDisabled());
    if (disabled_ == value) {
        return;
    }
    disabled_ = value;
    if (tabPane_ != nullptr) {
        tabPane_->syncAll();
    }
}

void Tab::assignPane(TabPane* pane) {
    tabPane_ = pane;
    updateDisabled();
}

void Tab::notifySelection() {
    if (onSelectionChanged_) {
        onSelectionChanged_();
    }
}

void Tab::notifyClosed() {
    if (onClosed_) {
        onClosed_();
    }
}

bool Tab::notifyCloseRequest() {
    if (!onCloseRequest_) {
        return false;
    }
    TabCloseRequest request;
    onCloseRequest_(request);
    return request.consumed;
}

void Tab::detachQuietly() {
    onSelectionChanged_ = nullptr;
    onClosed_ = nullptr;
    onCloseRequest_ = nullptr;
    tabPane_ = nullptr;
    selected_ = false;
    disabled_ = disable_;
}

void Tab::clearContent(Node* node) {
    if (content_.get() == node) {
        content_.reset();
    }
}

void Tab::clearGraphic(Node* node) {
    if (graphic_.get() == node) {
        graphic_.reset();
    }
}

}  // namespace jadefx
