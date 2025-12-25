#include "RTBEngine.h"

int main(int argc, char* argv[])
{
    using namespace RTBEngine;

    Core::ApplicationConfig config;
    config.window.title = "RTBEngine Demo";
    config.window.width = 600;
    config.window.height = 400;
    config.initialScenePath = "Assets/Scenes/TestScene.lua";

    return RTBEngine::Run(config);
}
