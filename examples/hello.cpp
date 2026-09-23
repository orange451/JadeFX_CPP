#include "jadefx/jadefx.hpp"

#include <memory>

namespace {

class HelloWorld : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto label = jadefx::make<jadefx::Label>("Hello World");
        label->setFont(jadefx::Font("Open Sans", 28.f));
        stage.setScene(jadefx::make<jadefx::Scene>(label, 320, 240));
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<HelloWorld>(), argc, argv);
}
