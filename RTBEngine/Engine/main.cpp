#include "RTBEngine.h"

int main(int argc, char* argv[])
{
    using namespace RTBEngine;

    Core::ApplicationConfig config;
    config.window.title = "RTBEngine Demo";
    config.window.width = 800;
    config.window.height = 600;
    config.initialScenePath = "Assets/Scenes/TestScene.lua";

    return RTBEngine::Run(config);
}
