
#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "Containers/Array.h"
#include "Interfaces/IModuleInterface.h"
#include "Interfaces/IRenderer.h"
#include "SmartPointers/UniquePtr.h"

class FEngine
{
public:
    
    static FEngine& Get();
    
    template <typename Module>
    void RegisterModule()
    {
        Modules.Emplace(std::make_unique<Module>()); 
    }

    template <typename Module>
    Module& GetModule(this FEngine& Core)
    {
        for (const auto& ModulePtr : Core.Modules)
        {
            if (auto CastedModule = dynamic_cast<Module*>(ModulePtr.get()))
            {
                return *CastedModule;
            }
        }
        throw std::runtime_error("Module not found");
    }
    
    void StartupModules();
    void ShutdownModules();
    
private:
    
    TArray<TUniquePtr<IModuleInterface>> Modules;
    
};
