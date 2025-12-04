#include "Core/Application.h"
#include <iostream>

int main(int argc, char* argv[])
{
    RTBEngine::Core::Application app;

    if (!app.Initialize()) {
        std::cerr << "Failed to initialize application!" << std::endl;
        return -1;
    }

    while (true)
    {
        app.Run();
    }

    return 0;
}
