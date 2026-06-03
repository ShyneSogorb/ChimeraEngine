
#pragma once

#include "Interfaces/IModuleInterface.h"
#include "Interfaces/IRenderer.h"
#include "SmartPointers/UniquePtr.h"


struct MVulkanRenderer : public IModuleInterface
{
    
    void ModuleStartup() override;
    void ModuleShutdown() override;
    
    [[nodiscard]] IRenderer& GetRenderer() const {return *Renderer;}
    
private:
    
    TUniquePtr<IRenderer> Renderer;
    
};


