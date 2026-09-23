#include "jadefx/application/Application.hpp"

#include <memory>

std::unique_ptr<jadefx::Application> createApplication();

#if defined(JADEFX_GLFM)
#define GLFM_INCLUDE_NONE
#include <glfm.h>

void glfmMain(GLFMDisplay* display) {
    jadefx::launchOnGlfm(display, createApplication());
}
#endif
