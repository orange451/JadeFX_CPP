#pragma once

namespace jadefx {

class GlfwHost;
class Stage;

void bindDesktopPrimary(GlfwHost& host, Stage& stage);
void shutdownDesktopWindows();
void closeFlaggedDesktopWindows();
bool drawDesktopWindows();
// An open utility window, such as Preferences or a floating panel, has the keyboard focus.
bool desktopWindowFocused();

}  // namespace jadefx
