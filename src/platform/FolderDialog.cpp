#include "jadefx/stage/FolderDialog.hpp"

#include "jadefx/application/RunLater.hpp"

#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#elif defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#else
#include <cerrno>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;
#define JADEFX_FOLDER_DIALOG_SPAWN 1
#endif

namespace jadefx {
namespace {

#if defined(_WIN32)

std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size > 0 ? size : 0), L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    }
    return wide;
}

std::string Narrow(const wchar_t* text) {
    if (text == nullptr || text[0] == L'\0') {
        return std::string();
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return std::string();
    }
    std::string narrow(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow.data(), size, nullptr, nullptr);
    return narrow;
}

// The common item dialog. Open picks a folder; Save names one. Modal to the
// active window, so input to the IDE waits until it closes.
DialogResult RunDialog(const FolderDialogOptions& options, std::string& path) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    // S_FALSE means COM was already up on this thread; it still needs a matching uninit.
    const bool owns = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        return DialogResult::Unavailable;
    }
    DialogResult result = DialogResult::Unavailable;
    IFileDialog* dialog = nullptr;
    const CLSID kind = options.save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
    if (SUCCEEDED(CoCreateInstance(kind, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        FILEOPENDIALOGOPTIONS flags = 0;
        dialog->GetOptions(&flags);
        flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
        if (options.save) {
            flags &= ~static_cast<FILEOPENDIALOGOPTIONS>(FOS_OVERWRITEPROMPT);
        } else {
            flags |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
        }
        dialog->SetOptions(flags);
        if (!options.title.empty()) {
            dialog->SetTitle(Widen(options.title).c_str());
        }
        if (!options.directory.empty()) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(Widen(options.directory).c_str(), nullptr,
                                                      IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }
        if (options.save && !options.name.empty()) {
            dialog->SetFileName(Widen(options.name).c_str());
        }
        const HRESULT shown = dialog->Show(GetActiveWindow());
        if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            result = DialogResult::Cancelled;
        } else if (SUCCEEDED(shown)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR chosen = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &chosen))) {
                    path = Narrow(chosen);
                    CoTaskMemFree(chosen);
                    result = path.empty() ? DialogResult::Cancelled : DialogResult::Chosen;
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (owns) {
        CoUninitialize();
    }
    return result;
}

#elif defined(JADEFX_FOLDER_DIALOG_SPAWN)

enum class Spawned { Chosen, Cancelled, Missing };

// Runs argv with stdout on a pipe. Missing: the program is not installed.
Spawned RunTool(const std::vector<std::string>& args, std::string& out) {
    std::vector<char*> argv;
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int pipeFds[2] = {-1, -1};
    if (pipe(pipeFds) != 0) {
        return Spawned::Missing;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
    pid_t pid = 0;
    const int spawned = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeFds[1]);
    if (spawned != 0) {
        close(pipeFds[0]);
        return Spawned::Missing;
    }
    char buffer[4096];
    for (;;) {
        const ssize_t got = read(pipeFds[0], buffer, sizeof(buffer));
        if (got > 0) {
            out.append(buffer, static_cast<std::size_t>(got));
        } else if (got == 0 || errno != EINTR) {
            break;
        }
    }
    close(pipeFds[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return Spawned::Missing;
        }
    }
    if (!WIFEXITED(status)) {
        return Spawned::Cancelled;
    }
    // 127: the shell-style "command not found" from a failed exec.
    if (WEXITSTATUS(status) == 127) {
        return Spawned::Missing;
    }
    if (WEXITSTATUS(status) != 0) {
        return Spawned::Cancelled;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }
    return out.empty() ? Spawned::Cancelled : Spawned::Chosen;
}

std::string JoinPath(const std::string& directory, const std::string& name) {
    if (directory.empty()) {
        return name;
    }
    if (directory.back() == '/') {
        return directory + name;
    }
    return directory + "/" + name;
}

DialogResult RunDialog(const FolderDialogOptions& options, std::string& path) {
    const std::string title = options.title.empty() ? std::string(options.save ? "Save As" : "Open") : options.title;
    // A trailing slash starts zenity inside the directory instead of selecting it.
    const std::string start =
        options.save ? JoinPath(options.directory, options.name) : JoinPath(options.directory, std::string());
    std::vector<std::string> zenity = {"zenity", "--file-selection", "--title=" + title};
    zenity.push_back(options.save ? "--save" : "--directory");
    if (!start.empty()) {
        zenity.push_back("--filename=" + start);
    }
    std::vector<std::string> kdialog = {"kdialog", "--title", title};
    kdialog.push_back(options.save ? "--getsavefilename" : "--getexistingdirectory");
    kdialog.push_back(start.empty() ? std::string(".") : start);
    // KDE sessions get kdialog first; everything else tries zenity first.
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    const bool kde = desktop != nullptr && std::string(desktop).find("KDE") != std::string::npos;
    const std::vector<std::string>* tools[2] = {kde ? &kdialog : &zenity, kde ? &zenity : &kdialog};
    for (const std::vector<std::string>* tool : tools) {
        std::string out;
        const Spawned spawned = RunTool(*tool, out);
        if (spawned == Spawned::Missing) {
            continue;
        }
        if (spawned == Spawned::Cancelled) {
            return DialogResult::Cancelled;
        }
        path = out;
        return DialogResult::Chosen;
    }
    return DialogResult::Unavailable;
}

#endif

}  // namespace

void showFolderDialog(FolderDialogOptions options, FolderDialogHandler done) {
    if (!done) {
        return;
    }
#if defined(_WIN32)
    // From the next frame, so the click that asked for it has finished.
    runLater([options = std::move(options), done = std::move(done)]() {
        std::string path;
        const DialogResult result = RunDialog(options, path);
        done(result, path);
    });
#elif defined(JADEFX_FOLDER_DIALOG_SPAWN)
    // The dialog is another process. Waiting here would stall the window long
    // enough for the desktop to call it unresponsive, so a thread waits instead.
    std::thread([options = std::move(options), done = std::move(done)]() mutable {
        std::string path;
        const DialogResult result = RunDialog(options, path);
        runLater([done = std::move(done), result, path = std::move(path)]() { done(result, path); });
    }).detach();
#else
    (void)options;
    runLater([done = std::move(done)]() { done(DialogResult::Unavailable, std::string()); });
#endif
}

}  // namespace jadefx
