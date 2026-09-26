#pragma once

#include <functional>
#include <string>

namespace jadefx {

struct FolderDialogOptions {
    std::string title;
    // Where the dialog starts. Empty uses the system default.
    std::string directory;
    // Open: the user picks an existing folder. Save: the user browses to a
    // location and types a name; that folder may not exist yet.
    bool save = false;
    // Save only. The name the dialog suggests.
    std::string name;
};

enum class DialogResult { Chosen, Cancelled, Unavailable };

using FolderDialogHandler = std::function<void(DialogResult result, const std::string& path)>;

// Shows the system folder dialog: NSOpenPanel or NSSavePanel on macOS, the
// common item dialog on Windows, zenity or kdialog on Linux and the BSDs.
// Returns at once. done runs later on the UI thread, through runLater. path is
// a UTF-8 absolute path when the result is Chosen. Unavailable means this
// platform has no system dialog: iOS, Android, the web, or a Linux desktop
// without zenity or kdialog.
void showFolderDialog(FolderDialogOptions options, FolderDialogHandler done);

}  // namespace jadefx
