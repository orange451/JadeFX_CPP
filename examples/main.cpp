#include "jadefx/application/Application.hpp"

#include <memory>

std::unique_ptr<jadefx::Application> createApplication();

int main(int argc, char** argv) {
    return jadefx::Application::launch(createApplication(), argc, argv);
}
