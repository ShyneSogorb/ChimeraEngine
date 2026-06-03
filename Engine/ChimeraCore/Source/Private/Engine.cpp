
#include "Engine.h"

#include <cassert>

FEngine& FEngine::Get()
{
    static FEngine Instance;
    return Instance;
}

void FEngine::StartupModules()
{
    for (auto& Module : Modules)
    {
        Module->ModuleStartup();
    }
}

void FEngine::ShutdownModules()
{
    for (auto& Module : Modules)
    {
        Module->ModuleShutdown();
    }
}
