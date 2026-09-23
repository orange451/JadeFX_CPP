#pragma once

#include <string>

namespace jadefx {

// The label and role of one dialog button. Alert orders and activates buttons by Data.
class ButtonType {
public:
    enum class Data { Left, Right, OkDone, CancelClose, Other };

    ButtonType(std::string text, Data data) : text_(std::move(text)), data_(data) {}

    static const ButtonType& Ok() {
        static const ButtonType value("OK", Data::OkDone);
        return value;
    }
    static const ButtonType& Cancel() {
        static const ButtonType value("Cancel", Data::CancelClose);
        return value;
    }
    static const ButtonType& Yes() {
        static const ButtonType value("Yes", Data::OkDone);
        return value;
    }
    static const ButtonType& No() {
        static const ButtonType value("No", Data::CancelClose);
        return value;
    }
    static const ButtonType& Close() {
        static const ButtonType value("Close", Data::CancelClose);
        return value;
    }

    const std::string& getText() const { return text_; }
    Data getButtonData() const { return data_; }

    bool operator==(const ButtonType& other) const { return text_ == other.text_ && data_ == other.data_; }
    bool operator!=(const ButtonType& other) const { return !(*this == other); }

private:
    std::string text_;
    Data data_;
};

}  // namespace jadefx
