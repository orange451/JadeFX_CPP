#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/Tab.hpp"

#include <cstddef>
#include <memory>

namespace jadefx {

// Pages of content with a strip of headers, in the shape of JavaFX TabPane.
// The selected tab's content fills the area beside the strip. The other pages
// stay out of the scene until they are selected. setSide places the strip on
// an edge; left and right keep the titles horizontal. Headers shrink to the
// strip, and a long title ends in an ellipsis.
//
// getTabClosingPolicy() decides which closable tabs draw a close button.
// The default shows it on the selected tab only.
// A right-click on a header offers Close, Close Others, and Close to the Right.
// Those actions close any closable tab, including one whose button is hidden
// because only the selected tab shows one. A tab that is not closable, a
// disabled tab, and TabClosingPolicy::Unavailable stay open. Close asks
// onCloseRequest first; consume() keeps the tab.
class TabPane : public Controls {
public:
    enum class TabClosingPolicy { SelectedTab, AllTabs, Unavailable };

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    TabPane();
    ~TabPane() override;

    const char* getElementType() const override { return "tabpane"; }

    ObservableList<std::shared_ptr<Tab>>& getTabs() { return tabs(); }
    const ObservableList<std::shared_ptr<Tab>>& getTabs() const { return tabs(); }

    void select(std::size_t index);
    void select(const std::shared_ptr<Tab>& tab);
    void select(Tab* tab);

    Tab* getSelectedTab() const;
    std::size_t getSelectedIndex() const;

    void setSide(Side side);
    Side getSide() const;

    void setTabClosingPolicy(TabClosingPolicy policy);
    TabClosingPolicy getTabClosingPolicy() const;

    void setTabMinWidth(double value);
    void setTabMaxWidth(double value);
    void setTabMinHeight(double value);
    void setTabMaxHeight(double value);
    double getTabMinWidth() const;
    double getTabMaxWidth() const;
    double getTabMinHeight() const;
    double getTabMaxHeight() const;

    // Grays the headers and blocks header clicks and close buttons.
    void setDisable(bool value);
    bool isDisabled() const;

protected:
    void layoutChildren() override;
    void visitChildren(const std::function<void(Node*)>& visitor) override;
    void detachChild(Node* child) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    class TabHeader;
    class HeaderBar;
    struct Impl;

    friend class Tab;

    ObservableList<std::shared_ptr<Tab>>& tabs();
    const ObservableList<std::shared_ptr<Tab>>& tabs() const;

    void onAdded(std::shared_ptr<Tab> tab, std::size_t index);
    void onRemoved(std::shared_ptr<Tab> tab, std::size_t index);
    void showSelected();
    void syncAll();
    void requestClose(const std::shared_ptr<Tab>& tab);
    void forget(Tab* tab);
    void noteContent(Tab& tab, const std::shared_ptr<Node>& previous);
    void releaseGraphic(Node* child);
    bool closeShown(const Tab& tab) const;
    // True when Close would remove the tab. The button policy is separate:
    // SelectedTab hides buttons on the other tabs, and the menu can still close them.
    bool canClose(const Tab& tab) const;
    void showTabMenu(Tab& tab, double x, double y);
    void closeOtherTabs(const std::shared_ptr<Tab>& keep);
    void closeTabsAfter(const std::shared_ptr<Tab>& origin);
    void selectNow(const std::shared_ptr<Tab>& tab);
    std::shared_ptr<TabHeader> makeHeader(const std::shared_ptr<Tab>& tab);

    std::unique_ptr<Impl> impl_;
};

}  // namespace jadefx
