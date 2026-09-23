#pragma once

#include "jadefx/collections/ObservableList.hpp"

#include <any>
#include <functional>
#include <memory>
#include <string>

namespace jadefx {

class Node;
class TabPane;

// Passed to Tab::setOnCloseRequest. consume() keeps the tab in the pane.
struct TabCloseRequest {
    bool consumed = false;
    void consume() { consumed = true; }
};

// One page in a TabPane. The tab itself is not a scene-graph node.
// getContent() is shown while the tab is selected. A close click asks
// onCloseRequest first; consume() leaves the tab, and onClosed runs after removal.
class Tab {
public:
    Tab();
    explicit Tab(std::string text);
    Tab(std::string text, std::shared_ptr<Node> content);
    ~Tab();

    Tab(const Tab&) = delete;
    Tab& operator=(const Tab&) = delete;
    Tab(Tab&&) = delete;
    Tab& operator=(Tab&&) = delete;

    void setId(std::string id);
    const std::string& getId() const { return id_; }

    void setStyle(std::string css);
    const std::string& getStyle() const { return style_; }

    // Includes the "tab" class. Copied onto the header so .tab and tab:selected match.
    ObservableList<std::string>& getStyleClass() { return styleClass_; }
    const ObservableList<std::string>& getStyleClass() const { return styleClass_; }

    void setText(std::string text);
    const std::string& getText() const { return text_; }

    void setGraphic(std::shared_ptr<Node> graphic);
    Node* getGraphic() const { return graphic_.get(); }

    void setContent(std::shared_ptr<Node> content);
    Node* getContent() const { return content_.get(); }

    // A tab that is not closable never draws a close button. The default is closable.
    void setClosable(bool closable);
    bool isClosable() const { return closable_; }

    bool isSelected() const { return selected_; }
    TabPane* getTabPane() const { return tabPane_; }

    // The header ignores clicks while disabled. select() can still show the page.
    // The page itself stays interactive. isDisabled() is also true when the TabPane is disabled.
    void setDisable(bool value);
    bool isDisable() const { return disable_; }
    bool isDisabled() const { return disabled_; }

    void setOnSelectionChanged(std::function<void()> handler) { onSelectionChanged_ = std::move(handler); }
    void setOnClosed(std::function<void()> handler) { onClosed_ = std::move(handler); }
    void setOnCloseRequest(std::function<void(TabCloseRequest&)> handler) { onCloseRequest_ = std::move(handler); }

    void setUserData(std::any value) { userData_ = std::move(value); }
    const std::any& getUserData() const { return userData_; }

private:
    friend class TabPane;

    void bindStyleList();
    void updateDisabled();
    void assignPane(TabPane* pane);
    void setSelectedFlag(bool value) { selected_ = value; }
    void notifySelection();
    void notifyClosed();
    bool notifyCloseRequest();
    void detachQuietly();

    const std::shared_ptr<Node>& contentNode() const { return content_; }
    const std::shared_ptr<Node>& graphicNode() const { return graphic_; }
    void clearContent(Node* node);
    void clearGraphic(Node* node);

    std::string id_;
    std::string style_;
    ObservableList<std::string> styleClass_;
    std::string text_;
    std::shared_ptr<Node> graphic_;
    std::shared_ptr<Node> content_;
    bool closable_ = true;
    bool selected_ = false;
    bool disable_ = false;
    bool disabled_ = false;
    TabPane* tabPane_ = nullptr;
    std::function<void()> onSelectionChanged_;
    std::function<void()> onClosed_;
    std::function<void(TabCloseRequest&)> onCloseRequest_;
    std::any userData_;
};

}  // namespace jadefx
