#include "ErrorDialog.hpp"

#include <cstdio>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace jadefx {
namespace {

#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

bool SpawnDialog(const char* const* argv) {
    pid_t pid = 0;
    if (posix_spawnp(&pid, argv[0], nullptr, nullptr, const_cast<char* const*>(argv), environ) != 0) {
        return false;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

#endif

}  // namespace

void ShowErrorDialog(const std::string& title, const std::string& message) {
#if defined(_WIN32)
    auto widen = [](const std::string& text) {
        if (text.empty()) {
            return std::wstring();
        }
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(size > 0 ? size : 0), L'\0');
        if (size > 0) {
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
        }
        return wide;
    };
    const std::wstring wideTitle = widen(title.empty() ? "JadeFX" : title);
    const std::wstring wideMessage = widen(message);
    MessageBoxW(nullptr, wideMessage.c_str(), wideTitle.c_str(),
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
#elif defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "JadeFX", "%s\n%s", title.c_str(), message.c_str());
#elif defined(__EMSCRIPTEN__)
    EM_ASM(
        {
            var title = UTF8ToString($0);
            var message = UTF8ToString($1);
            if (typeof window !== "undefined" && window.alert) {
                window.alert(title + "\n\n" + message);
            }
        },
        title.c_str(), message.c_str());
#else
    const char* heading = title.c_str();
    const char* body = message.c_str();
    const char* zenity[] = {"zenity", "--error", "--no-wrap", "--ok-label", "Quit", "--title", heading, "--text", body,
                            nullptr};
    const char* kdialog[] = {"kdialog", "--title", heading, "--error", body, nullptr};
    const char* xmessage[] = {"xmessage", "-center", "-buttons", "Quit:0", "-title", heading, body, nullptr};
    if (SpawnDialog(zenity) || SpawnDialog(kdialog) || SpawnDialog(xmessage)) {
        return;
    }
    std::fprintf(stderr, "%s\n%s\n", heading, body);
#endif
}

}  // namespace jadefx
