//
// Created by user on 26/05/2026.
//

#include "MainModule.h"

#include "Engine.h"
#include "VulkanRenderer.h"

void MMainModule::ModuleStartup()
{
    FEngine::Get().RegisterModule<MVulkanRenderer>();
    
    FEngine::Get().StartupModules();
}

void MMainModule::ModuleShutdown()
{
    FEngine::Get().ShutdownModules();
}