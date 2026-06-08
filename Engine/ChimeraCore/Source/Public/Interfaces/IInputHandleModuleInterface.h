
#pragma once

#include "IModuleInterface.h"
#include "Macros/CompilerAbstractions.h"

struct IInputHandleModuleInterface : public IModuleInterface
{
    
    virtual void ModuleStartup(){};
    virtual void ModuleShutdown(){};
    
    NO_DISCARD virtual class IInputHandler& GetInputHandler() const = 0;
    
    virtual ~IInputHandleModuleInterface() = default;
    
};

class IInputHandler
{
public:
    virtual ~IInputHandler() = default;
    
    virtual void UpdateInputs() = 0;
    virtual bool RequestQuit() const = 0;
    
};



