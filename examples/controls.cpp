#include "jadefx/jadefx.hpp"

#include "jadefx/scene/Controls/Alert.hpp"
#include "jadefx/scene/Controls/ButtonType.hpp"
#include "jadefx/scene/Controls/CheckBox.hpp"
#include "jadefx/scene/Controls/ComboBox.hpp"
#include "jadefx/scene/Controls/Menu.hpp"
#include "jadefx/scene/Controls/MenuBar.hpp"
#include "jadefx/scene/Controls/MenuButton.hpp"
#include "jadefx/scene/Controls/MenuItem.hpp"
#include "jadefx/scene/Controls/ProgressBar.hpp"
#include "jadefx/scene/Controls/RadioButton.hpp"
#include "jadefx/scene/Controls/Slider.hpp"
#include "jadefx/scene/Controls/Spinner.hpp"
#include "jadefx/scene/Controls/TextField.hpp"
#include "jadefx/scene/Controls/ToggleButton.hpp"
#include "jadefx/scene/Controls/ToggleGroup.hpp"
#include "jadefx/scene/Controls/Tooltip.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #e8eaed;
    font-family: "Open Sans";
    font-size: 15px;
    color: #202124;
}
.sheet {
    background-color: white;
    border-radius: 12px;
    border-width: 1px;
    border-color: #dadce0;
    padding: 20px;
    spacing: 14px;
}
.row {
    spacing: 10px;
    alignment: center-left;
}
.hint {
    color: #5f6368;
    font-size: 13px;
}
button, menubutton, togglebutton {
    background-color: white;
    border-width: 1px;
    border-color: #dadce0;
    border-radius: 6px;
    padding: 6px 14px;
}
button:hover, menubutton:hover, togglebutton:hover {
    background-color: #f8f9fa;
}
togglebutton:selected {
    background-color: #e8f0fe;
    border-color: #1a73e8;
    color: #174ea6;
}
checkbox:selected, checkbox:indeterminate {
    color: #174ea6;
}
textfield, combobox, spinner {
    background-color: white;
    border-width: 1px;
    border-color: #dadce0;
    border-radius: 6px;
    padding: 6px 8px;
}
combobox textfield, spinner textfield {
    padding: 0 2px;
    border-width: 0;
    background-color: transparent;
}
textfield:focus, combobox:focus, combobox:focus-within, spinner:focus, spinner:focus-within {
    border-color: #1a73e8;
    border-width: 2px;
}
increment-arrow-button:hover, decrement-arrow-button:hover {
    background-color: #f1f3f4;
}
menubar {
    background-color: white;
    border-width: 0 0 1px 0;
    border-color: #dadce0;
    padding: 0 8px;
}
menu:hover, menu-item:hover, combo-row:hover {
    background-color: #e8f0fe;
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
        greet->setOnAction([name](jadefx::ActionEvent&) { name->fire(); });

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
        auto dialogRow = jadefx::make<jadefx::HBox>();
        dialogRow->getClassList().add("row");
        dialogRow->setSpacing(10);
        dialogRow->getChildren().add(info);
        dialogRow->getChildren().add(warn);
        dialogRow->getChildren().add(confirm);
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
        root->setCenter(sheet);
        root->setPadding(jadefx::Insets::uniform(16));

        auto scene = jadefx::make<jadefx::Scene>(root, 720, 780);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Controls");
        stage.setScene(scene);
    }

private:
    std::shared_ptr<jadefx::ToggleGroup> sizes_;
    std::shared_ptr<std::vector<std::shared_ptr<jadefx::Alert>>> alerts_;
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<ControlsApp>(), argc, argv);
}
