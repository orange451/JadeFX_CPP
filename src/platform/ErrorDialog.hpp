#pragma once

#include <string>

namespace jadefx {

// Modal error dialog. Returns after the user dismisses it.
void ShowErrorDialog(const std::string& title, const std::string& message);

}  // namespace jadefx
