#include "core/Application.h"
#include "core/Log.h"

#include <iostream>

int main()
{
    try
    {
        lol::Log::Initialize();
        lol::Application app;
        app.Run();
    }
    catch (const std::exception& ex)
    {
        lol::Log::Push(lol::LogLevel::Error, ex.what());
        std::cerr << "Startup failed: " << ex.what() << '\n';
        std::cerr << "See log file: " << lol::Log::LogFilePath().string() << '\n';
        return 1;
    }

    return 0;
}
