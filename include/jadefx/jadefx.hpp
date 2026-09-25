#pragma once

#include "jadefx/application/Application.hpp"
#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/event/Events.hpp"
#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/Node.hpp"
#include "jadefx/scene/image/Image.hpp"
#include "jadefx/scene/image/ImageView.hpp"
#include "jadefx/scene/Parent.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/controls/Alert.hpp"
#include "jadefx/scene/controls/Button.hpp"
#include "jadefx/scene/controls/ButtonType.hpp"
#include "jadefx/scene/controls/ComboBox.hpp"
#include "jadefx/scene/controls/CheckBox.hpp"
#include "jadefx/scene/controls/RadioButton.hpp"
#include "jadefx/scene/controls/ToggleButton.hpp"
#include "jadefx/scene/controls/ToggleGroup.hpp"
#include "jadefx/scene/controls/Tooltip.hpp"
#include "jadefx/scene/controls/CodeArea.hpp"
#include "jadefx/scene/controls/InlineCssTextArea.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/controls/Menu.hpp"
#include "jadefx/scene/controls/MenuBar.hpp"
#include "jadefx/scene/controls/MenuButton.hpp"
#include "jadefx/scene/controls/MenuItem.hpp"
#include "jadefx/scene/controls/ProgressBar.hpp"
#include "jadefx/scene/controls/StyleClassedTextArea.hpp"
#include "jadefx/scene/controls/Slider.hpp"
#include "jadefx/scene/controls/Spinner.hpp"
#include "jadefx/scene/controls/SplitPane.hpp"
#include "jadefx/scene/controls/StyledTextArea.hpp"
#include "jadefx/scene/controls/Tab.hpp"
#include "jadefx/scene/controls/TextField.hpp"
#include "jadefx/scene/controls/TabPane.hpp"
#include "jadefx/scene/controls/TreeItem.hpp"
#include "jadefx/scene/controls/TreeView.hpp"
#include "jadefx/scene/layout/BorderPane.hpp"
#include "jadefx/scene/layout/HBox.hpp"
#include "jadefx/scene/layout/Pane.hpp"
#include "jadefx/scene/layout/StackPane.hpp"
#include "jadefx/scene/layout/VBox.hpp"
#include "jadefx/scene/text/EditableStyledDocument.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "jadefx/scene/text/StyleSpans.hpp"
#include "jadefx/scene/text/StyledDocument.hpp"
#include "jadefx/scene/text/TextStyle.hpp"
#include "jadefx/stage/Stage.hpp"
#include "jadefx/stage/UtilityWindow.hpp"
#include "jadefx/style/Style.hpp"

#include <memory>
#include <utility>

namespace jadefx {

template <typename T, typename... Args>
std::shared_ptr<T> make(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

}  // namespace jadefx
