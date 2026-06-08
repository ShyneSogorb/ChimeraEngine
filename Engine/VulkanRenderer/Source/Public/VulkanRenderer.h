
#pragma once

#include "Interfaces/IRenderModuleInterface.h"
#include "Macros/CompilerAbstractions.h"
#include "SmartPointers/UniquePtr.h"


struct MVulkanRenderer : public IRenderModuleInterface
{
    
    void ModuleStartup() override;
    void ModuleShutdown() override;
    
    NO_DISCARD IRenderer& GetRenderer() const override {return *Renderer;}
    
private:
    
    TUniquePtr<IRenderer> Renderer;
    
};


