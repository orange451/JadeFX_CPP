#pragma once

namespace jadefx {

class GlfwHost;
class Stage;

void bindDesktopPrimary(GlfwHost& host, Stage& stage);
void shutdownDesktopWindows();
void closeFlaggedDesktopWindows();
bool drawDesktopWindows();

}  // namespace jadefx
