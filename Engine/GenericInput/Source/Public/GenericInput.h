
#pragma once

#include "Interfaces/IInputHandleModuleInterface.h"
#include "SmartPointers/UniquePtr.h"

class MGenericInput : public IInputHandleModuleInterface
{
    
public:
    virtual void StartupModule();
    virtual void ShutdownModule(){};
    
    NO_DISCARD IInputHandler& GetInputHandler() const override {return *InputHandle;}
    
private:
    
    TUniquePtr<IInputHandler> InputHandle;
    
};
