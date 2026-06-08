
#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "Containers/Array.h"
#include "Containers/Stack.h"
#include "Interfaces/IModuleInterface.h"
#include "Interfaces/IRenderer.h"
#include "SmartPointers/UniquePtr.h"

template <typename T>
concept ModuleInterface = std::is_base_of_v<IModuleInterface, T>;

class FEngine
{
public:
    
    static FEngine& Get();
    
    template <ModuleInterface Module>
    void RegisterModule()
    {
        Modules.Emplace(MakeUnique<Module>()); 
    }

    template <ModuleInterface Module>
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
    
    TStack<TUniquePtr<IModuleInterface>> Modules;
    
};
