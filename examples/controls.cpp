#include "jadefx/jadefx.hpp"

#include "jadefx/scene/controls/Alert.hpp"
#include "jadefx/scene/controls/ButtonType.hpp"
#include "jadefx/scene/controls/CheckBox.hpp"
#include "jadefx/scene/controls/ColorPicker.hpp"
#include "jadefx/scene/controls/ComboBox.hpp"
#include "jadefx/scene/controls/DatePicker.hpp"
#include "jadefx/scene/controls/Notifications.hpp"
#include "jadefx/scene/controls/Menu.hpp"
#include "jadefx/scene/controls/MenuBar.hpp"
#include "jadefx/scene/controls/MenuButton.hpp"
#include "jadefx/scene/controls/MenuItem.hpp"
#include "jadefx/scene/controls/ProgressBar.hpp"
#include "jadefx/scene/controls/RadioButton.hpp"
#include "jadefx/scene/controls/Slider.hpp"
#include "jadefx/scene/controls/Spinner.hpp"
#include "jadefx/scene/controls/TextField.hpp"
#include "jadefx/scene/controls/ToggleButton.hpp"
#include "jadefx/scene/controls/ToggleGroup.hpp"
#include "jadefx/scene/controls/Tooltip.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

// Colors come from the theme's custom properties, so the Dark theme box
// restyles the whole window.
constexpr const char* kStylesheet = R"CSS(
scene {
    font-family: "Open Sans";
    font-size: 15px;
}
.sheet {
    background-color: var(--surface-color);
    border-radius: 12px;
    border-width: 1px;
    border-color: var(--border-color);
    padding: 20px;
    spacing: 14px;
}
.row {
    spacing: 10px;
    alignment: center-left;
}
.chip {
    background-color: var(--selection-color);
    border-radius: 12px;
    padding: 4px 12px;
    cursor: grab;
}
.drop-zone {
    border-width: 1px;
    border-style: solid;
    border-color: var(--border-color);
    border-radius: 6px;
    padding: 8px 16px;
    color: var(--muted-color);
}
.drop-zone.over {
    border-color: var(--accent-color);
    background-color: var(--selection-color);
}
.hint {
    color: var(--muted-color);
    font-size: 13px;
}
button, menubutton, togglebutton {
    border-width: 1px;
    border-color: var(--border-color);
    border-radius: 6px;
    padding: 6px 14px;
}
button:hover, menubutton:hover, togglebutton:hover {
    background-color: var(--subtle-color);
}
togglebutton:selected {
    background-color: var(--selection-color);
    border-color: var(--accent-color);
    color: var(--accent-color);
}
checkbox:selected, checkbox:indeterminate {
    color: var(--accent-color);
}
textfield, combobox, spinner {
    border-width: 1px;
    border-color: var(--border-color);
    border-radius: 6px;
    padding: 6px 8px;
}
combobox textfield, spinner textfield {
    padding: 0 2px;
    border-width: 0;
    background-color: transparent;
}
textfield:focus, combobox:focus, combobox:focus-within, spinner:focus, spinner:focus-within {
    border-color: var(--accent-color);
    border-width: 2px;
}
increment-arrow-button:hover, decrement-arrow-button:hover {
    background-color: var(--track-color);
}
menubar {
    border-width: 0 0 1px 0;
    border-color: var(--border-color);
    padding: 0 8px;
}
menu:hover, menu-item:hover, combo-row:hover {
    background-color: var(--selection-color);
}
)CSS";

class ControlsApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        // The window outlives start(), and these objects are not owned by the scene graph.
        sizes_ = std::make_shared<jadefx::ToggleGroup>();
        alerts_ = std::make_shared<std::vector<std::shared_ptr<jadefx::Alert>>>();
        auto status = jadefx::make<jadefx::Label>("Hover the caption. Menus, fields, and dialogs report here.");
        status->getClassList().add("hint");

        auto file = jadefx::make<jadefx::Menu>("File");
        auto create = jadefx::make<jadefx::MenuItem>("New note");
        create->setAccelerator(jadefx::Key::N, jadefx::Key::ModControl);
        create->setOnAction([status](jadefx::ActionEvent&) { status->setText("New note"); });
        auto quit = jadefx::make<jadefx::MenuItem>("Quit");
        quit->setAccelerator(jadefx::Key::Q, jadefx::Key::ModControl);
        quit->setOnAction([status](jadefx::ActionEvent&) { status->setText("Quit stays in the sample"); });
        file->getItems().add(create);
        file->getItems().add(jadefx::make<jadefx::SeparatorMenuItem>());
        file->getItems().add(quit);

        auto edit = jadefx::make<jadefx::Menu>("Edit");
        auto cut = jadefx::make<jadefx::MenuItem>("Cut");
        cut->setDisable(true);
        auto caseMenu = jadefx::make<jadefx::Menu>("Change case");
        caseMenu->getItems().add(jadefx::make<jadefx::MenuItem>("Uppercase"));
        caseMenu->getItems().add(jadefx::make<jadefx::MenuItem>("Lowercase"));
        caseMenu->getItems()[0]->setOnAction([status](jadefx::ActionEvent&) { status->setText("Uppercase"); });
        caseMenu->getItems()[1]->setOnAction([status](jadefx::ActionEvent&) { status->setText("Lowercase"); });
        edit->getItems().add(cut);
        edit->getItems().add(caseMenu);

        auto bar = jadefx::make<jadefx::MenuBar>();
        bar->getMenus().add(file);
        bar->getMenus().add(edit);

        auto name = jadefx::make<jadefx::TextField>();
        name->setPromptText("Your name");
        name->setPrefColumnCount(18);
        name->setOnAction([name, status](jadefx::ActionEvent&) {
            status->setText(name->getText().empty() ? "The field is empty" : "Hello, " + name->getText());
        });
        auto greet = jadefx::make<jadefx::Button>("Greet");
        jadefx::Button* greetButton = greet.get();
        greet->setOnAction([name, greetButton](jadefx::ActionEvent&) {
            name->fire();
            jadefx::Toast::show(*greetButton, name->getText().empty() ? "Type a name first" : "Hello, " + name->getText());
        });

        auto city = jadefx::make<jadefx::ComboBox>();
        city->setPromptText("City");
        city->getItems().add("Lisbon");
        city->getItems().add("Kyoto");
        city->getItems().add("Montreal");
        city->getItems().add("Nairobi");
        city->setOnAction([city, status](jadefx::ActionEvent&) { status->setText("City: " + city->getValue()); });

        auto custom = jadefx::make<jadefx::ComboBox>();
        custom->setEditable(true);
        custom->setPromptText("Type or choose");
        custom->getItems().add("Morning");
        custom->getItems().add("Afternoon");
        custom->getItems().add("Evening");
        custom->setOnAction([custom, status](jadefx::ActionEvent&) { status->setText("When: " + custom->getValue()); });

        auto accent = jadefx::make<jadefx::ColorPicker>(jadefx::Color::parse("#1a73e8"));
        accent->setOnAction([accent, status](jadefx::ActionEvent&) {
            status->setText("Color: " + accent->getValue().toHex(accent->getValue().a < 1.f));
        });

        auto due = jadefx::make<jadefx::DatePicker>();
        due->setPromptText("Due date");
        due->setShowWeekNumbers(true);
        due->setOnAction([due, status](jadefx::ActionEvent&) {
            status->setText(due->getValue() ? "Due: " + due->getValue()->toString() : "No due date");
        });

        // Drag the chip onto the zone, or drop files on it from the system.
        auto chip = jadefx::make<jadefx::Label>("Drag me");
        chip->getClassList().add("chip");
        jadefx::Label* chipLabel = chip.get();
        chip->setOnDragDetected([chipLabel](const jadefx::MouseEvent&) {
            jadefx::Dragboard* board = chipLabel->startDragAndDrop(jadefx::TransferModes::copyOrMove());
            board->putString(chipLabel->getText());
            auto view = jadefx::make<jadefx::Label>(chipLabel->getText());
            view->getClassList().add("chip");
            board->setDragView(view, 12, 12);
        });
        auto zone = jadefx::make<jadefx::Label>("Drop text or files here");
        zone->getClassList().add("drop-zone");
        jadefx::Label* zoneLabel = zone.get();
        zone->setOnDragEntered([zoneLabel](jadefx::DragEvent&) { zoneLabel->getClassList().add("over"); });
        zone->setOnDragExited([zoneLabel](jadefx::DragEvent&) {
            zoneLabel->getClassList().removeIf([](const std::string& name) { return name == "over"; });
        });
        zone->setOnDragOver([](jadefx::DragEvent& event) {
            if (event.getDragboard().hasString() || event.getDragboard().hasFiles()) {
                event.acceptTransferModes(jadefx::TransferModes::copyOrMove());
            }
            event.consume();
        });
        zone->setOnDragDropped([zoneLabel](jadefx::DragEvent& event) {
            const jadefx::Dragboard& board = event.getDragboard();
            zoneLabel->setText(board.hasFiles() ? std::to_string(board.getFiles().size()) + " file(s): " + board.getFiles()[0]
                                                : "Dropped: " + board.getString());
            event.setDropCompleted(true);
            event.consume();
        });
        auto dragRow = jadefx::make<jadefx::HBox>();
        dragRow->getClassList().add("row");
        dragRow->setSpacing(10);
        dragRow->getChildren().add(chip);
        dragRow->getChildren().add(zone);

        auto small = jadefx::make<jadefx::RadioButton>("Small");
        auto medium = jadefx::make<jadefx::RadioButton>("Medium");
        auto large = jadefx::make<jadefx::RadioButton>("Large");
        small->setToggleGroup(sizes_.get());
        medium->setToggleGroup(sizes_.get());
        large->setToggleGroup(sizes_.get());
        medium->setSelected(true);
        auto onSize = [status](const std::string& label) {
            return [status, label](jadefx::ActionEvent&) { status->setText("Size: " + label); };
        };
        small->setOnAction(onSize("Small"));
        medium->setOnAction(onSize("Medium"));
        large->setOnAction(onSize("Large"));

        auto bold = jadefx::make<jadefx::ToggleButton>("Bold");
        auto italic = jadefx::make<jadefx::ToggleButton>("Italic");
        auto reportStyle = [bold, italic, status](jadefx::ActionEvent&) {
            std::string text = "Style:";
            if (bold->isSelected()) {
                text += " bold";
            }
            if (italic->isSelected()) {
                text += " italic";
            }
            if (!bold->isSelected() && !italic->isSelected()) {
                text += " none";
            }
            status->setText(text);
        };
        bold->setOnAction(reportStyle);
        italic->setOnAction(reportStyle);

        auto email = jadefx::make<jadefx::CheckBox>("Email");
        auto push = jadefx::make<jadefx::CheckBox>("Push");
        auto pages = jadefx::make<jadefx::CheckBox>("All pages");
        auto dark = jadefx::make<jadefx::CheckBox>("Dark theme");
        dark->setOnAction([this, dark](jadefx::ActionEvent&) {
            if (auto scene = scene_.lock()) {
                scene->setUserAgentStylesheet(dark->isSelected() ? jadefx::Theme::DARK : jadefx::Theme::LIGHT);
            }
        });
        email->setSelected(true);
        pages->setAllowIndeterminate(true);
        pages->setIndeterminate(true);
        auto reportNotify = [email, push, pages, status](jadefx::ActionEvent&) {
            std::string text;
            if (email->isSelected()) {
                text += "email";
            }
            if (push->isSelected()) {
                if (!text.empty()) {
                    text += ", ";
                }
                text += "push";
            }
            if (!text.empty()) {
                text += "; ";
            }
            if (pages->isIndeterminate()) {
                text += "pages mixed";
            } else if (pages->isSelected()) {
                text += "all pages";
            } else {
                text += "no pages";
            }
            status->setText(text);
        };
        email->setOnAction(reportNotify);
        push->setOnAction(reportNotify);
        pages->setOnAction(reportNotify);

        auto volume = jadefx::make<jadefx::Slider>(0, 100, 40);
        volume->setPrefWidth(220);
        volume->setShowTickMarks(true);
        volume->setShowTickLabels(true);
        volume->setMajorTickUnit(25);
        volume->setBlockIncrement(5);
        volume->setOnValueChanged([status, volume] {
            const int shown = static_cast<int>(std::lround(volume->getValue()));
            std::string text = "Volume " + std::to_string(shown);
            if (volume->isValueChanging()) {
                text += " (adjusting)";
            }
            status->setText(text);
        });
        auto copies = std::make_shared<jadefx::IntegerSpinnerValueFactory>(1, 12, 1);
        auto count = jadefx::make<jadefx::Spinner>(copies);
        count->setEditable(true);
        count->getEditor()->setPrefColumnCount(3);
        count->setOnValueChanged([status, copies] { status->setText("Copies: " + std::to_string(copies->getValue())); });

        auto more = jadefx::make<jadefx::MenuButton>("More");
        auto pin = jadefx::make<jadefx::MenuItem>("Pin this note");
        pin->setOnAction([status](jadefx::ActionEvent&) { status->setText("Pinned"); });
        more->getItems().add(pin);
        more->getItems().add(jadefx::make<jadefx::SeparatorMenuItem>());
        auto archive = jadefx::make<jadefx::MenuItem>("Archive");
        archive->setDisable(true);
        more->getItems().add(archive);

        auto info = jadefx::make<jadefx::Button>("Information");
        auto warn = jadefx::make<jadefx::Button>("Warning");
        auto confirm = jadefx::make<jadefx::Button>("Confirm");
        auto keep = alerts_;
        auto present = [status, keep](const std::shared_ptr<jadefx::Alert>& alert) {
            keep->push_back(alert);
            if (status->getScene() != nullptr) {
                alert->show(*status->getScene());
            }
        };
        info->setOnAction([present](jadefx::ActionEvent&) {
            auto alert = std::make_shared<jadefx::Alert>(jadefx::AlertType::Information, "The note was saved.");
            alert->setTitle("Saved");
            present(alert);
        });
        warn->setOnAction([present](jadefx::ActionEvent&) {
            auto alert = std::make_shared<jadefx::Alert>(jadefx::AlertType::Warning, "This draft has not been sent.");
            alert->setHeaderText("Unsent changes");
            present(alert);
        });
        confirm->setOnAction([status, present](jadefx::ActionEvent&) {
            auto alert = std::make_shared<jadefx::Alert>(jadefx::AlertType::Confirmation, "Delete this note?");
            alert->setOnClosed([status](const jadefx::ButtonType* type) {
                if (type != nullptr && *type == jadefx::ButtonType::Ok()) {
                    status->setText("Note deleted");
                } else {
                    status->setText("Delete canceled");
                }
            });
            present(alert);
        });


        auto saving = jadefx::make<jadefx::ProgressBar>(0.25);
        saving->setPrefWidth(280);
        auto step = jadefx::make<jadefx::Button>("Step");
        step->setOnAction([saving, status](jadefx::ActionEvent&) {
            double next = saving->getProgress() + 0.25;
            if (next > 1) {
                next = 0;
            }
            saving->setProgress(next);
            const int percent = static_cast<int>(next * 100 + 0.5);
            status->setText("Saved " + std::to_string(percent) + "%");
        });
        auto working = jadefx::make<jadefx::ProgressBar>();
        working->setPrefWidth(280);

        auto caption = jadefx::make<jadefx::Label>("A caption with a tooltip");
        jadefx::Tooltip::install(caption.get(), jadefx::make<jadefx::Tooltip>("Shown after the pointer rests here"));

        auto nameRow = jadefx::make<jadefx::HBox>();
        nameRow->getClassList().add("row");
        nameRow->setSpacing(10);
        nameRow->getChildren().add(name);
        nameRow->getChildren().add(greet);
        auto cityRow = jadefx::make<jadefx::HBox>();
        cityRow->getClassList().add("row");
        cityRow->setSpacing(10);
        cityRow->getChildren().add(city);
        cityRow->getChildren().add(custom);
        auto pickRow = jadefx::make<jadefx::HBox>();
        pickRow->getClassList().add("row");
        pickRow->setSpacing(10);
        pickRow->getChildren().add(accent);
        pickRow->getChildren().add(due);
        auto sizeRow = jadefx::make<jadefx::HBox>();
        sizeRow->getClassList().add("row");
        sizeRow->setSpacing(16);
        sizeRow->getChildren().add(small);
        sizeRow->getChildren().add(medium);
        sizeRow->getChildren().add(large);
        auto styleRow = jadefx::make<jadefx::HBox>();
        styleRow->getClassList().add("row");
        styleRow->setSpacing(10);
        styleRow->getChildren().add(bold);
        styleRow->getChildren().add(italic);
        styleRow->getChildren().add(more);
        auto notifyRow = jadefx::make<jadefx::HBox>();
        notifyRow->getClassList().add("row");
        notifyRow->setSpacing(16);
        notifyRow->getChildren().add(email);
        notifyRow->getChildren().add(push);
        notifyRow->getChildren().add(pages);
        notifyRow->getChildren().add(dark);
        auto dialogRow = jadefx::make<jadefx::HBox>();
        dialogRow->getClassList().add("row");
        dialogRow->setSpacing(10);
        dialogRow->getChildren().add(info);
        dialogRow->getChildren().add(warn);
        dialogRow->getChildren().add(confirm);
        auto notify = jadefx::make<jadefx::Button>("Notify");
        jadefx::Button* notifyButton = notify.get();
        notify->setOnAction([notifyButton, status](jadefx::ActionEvent&) {
            jadefx::Notifications::create()
                .title("Export finished")
                .text("scene.json was written.\nIt took 1.2 seconds.")
                .owner(*notifyButton)
                .action("Show", [status](jadefx::ActionEvent&) { status->setText("Showing scene.json"); })
                .onAction([status](jadefx::ActionEvent&) { status->setText("Notification clicked"); })
                .showInformation();
        });
        dialogRow->getChildren().add(notify);
        auto saveRow = jadefx::make<jadefx::HBox>();
        saveRow->getClassList().add("row");
        saveRow->setSpacing(10);
        saveRow->getChildren().add(jadefx::make<jadefx::Label>("Saving"));
        saveRow->getChildren().add(saving);
        saveRow->getChildren().add(step);
        auto workRow = jadefx::make<jadefx::HBox>();
        workRow->getClassList().add("row");
        workRow->setSpacing(10);
        workRow->getChildren().add(jadefx::make<jadefx::Label>("Working"));
        workRow->getChildren().add(working);

        auto adjustRow = jadefx::make<jadefx::HBox>();
        adjustRow->getClassList().add("row");
        adjustRow->setSpacing(10);
        adjustRow->getChildren().add(jadefx::make<jadefx::Label>("Volume"));
        adjustRow->getChildren().add(volume);
        adjustRow->getChildren().add(jadefx::make<jadefx::Label>("Copies"));
        adjustRow->getChildren().add(count);

        auto sheet = jadefx::make<jadefx::VBox>();
        sheet->getClassList().add("sheet");
        sheet->setSpacing(14);
        sheet->getChildren().add(nameRow);
        sheet->getChildren().add(cityRow);
        sheet->getChildren().add(pickRow);
        sheet->getChildren().add(dragRow);
        sheet->getChildren().add(sizeRow);
        sheet->getChildren().add(styleRow);
        sheet->getChildren().add(notifyRow);
        sheet->getChildren().add(dialogRow);
        sheet->getChildren().add(saveRow);
        sheet->getChildren().add(workRow);
        sheet->getChildren().add(adjustRow);
        sheet->getChildren().add(caption);
        sheet->getChildren().add(status);

        auto root = jadefx::make<jadefx::BorderPane>();
        root->setTop(bar);
        // The form scrolls when the window is shorter than it.
        auto scroller = jadefx::make<jadefx::ScrollPane>(sheet);
        scroller->setFitToWidth(true);
        root->setCenter(scroller);
        root->setPadding(jadefx::Insets::uniform(16));

        auto scene = jadefx::make<jadefx::Scene>(root, 720, 780);
        scene->setStylesheet(kStylesheet);
        scene_ = scene;
        stage.setTitle("Controls");
        stage.setScene(scene);
    }

private:
    std::shared_ptr<jadefx::ToggleGroup> sizes_;
    std::weak_ptr<jadefx::Scene> scene_;
    std::shared_ptr<std::vector<std::shared_ptr<jadefx::Alert>>> alerts_;
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<ControlsApp>(), argc, argv);
}
