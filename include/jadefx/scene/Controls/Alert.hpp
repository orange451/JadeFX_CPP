#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/Controls/ButtonType.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

class Button;
class Scene;

enum class AlertType { None, Information, Warning, Confirmation, Error };

// Modal message dialog. It is not a node: show() builds a dimmer popup on the scene.
// The scene must outlive the alert while that popup is showing.
class Alert {
public:
    explicit Alert(AlertType type);
    // An empty button list installs the type's defaults. A non-empty list is used as given.
    Alert(AlertType type, std::string contentText, std::vector<ButtonType> buttons = {});
    ~Alert();

    Alert(const Alert&) = delete;
    Alert& operator=(const Alert&) = delete;

    void setTitle(std::string title);
    const std::string& getTitle() const;
    void setHeaderText(std::string text);
    const std::string& getHeaderText() const;
    void setContentText(std::string text);
    const std::string& getContentText() const;
    AlertType getAlertType() const;

    ObservableList<ButtonType>& getButtonTypes();

    // Null until show(), or when that type was not built. Valid until the next show().
    Button* lookupButton(const ButtonType& type) const;

    // Called after the dialog hides. The pointer is owned by the alert, or null when there is no result.
    void setOnClosed(std::function<void(const ButtonType*)> handler);
    const ButtonType* getResult() const;

    // Records the result, hides the popup, then invokes the closed handler.
    void setResult(const ButtonType& type);

    void show(Scene& scene);

    // show(), then Scene::runEventPump until a result is set.
    // If a pump turn is not 1, returns the current result and leaves the dialog up when it is still null.
    const ButtonType* showAndWait(Scene& scene);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace jadefx
