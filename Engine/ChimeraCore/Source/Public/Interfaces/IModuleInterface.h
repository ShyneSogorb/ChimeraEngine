
#pragma once

struct IModuleInterface
{
    
    virtual void ModuleStartup(){};
    virtual void ModuleShutdown(){};
    
    virtual ~IModuleInterface() = default;
    
};


