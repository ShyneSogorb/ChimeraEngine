
#pragma once
#include "Interfaces/IModuleInterface.h"

struct  MChimeraCore : public IModuleInterface
{
    
    virtual void ModuleStartup();
    virtual void ModuleShutdown();
};


