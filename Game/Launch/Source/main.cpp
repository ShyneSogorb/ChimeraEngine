
#include "Engine.h"
#include "Interfaces/IRenderer.h"
#include "MainModule.h"
#include "VulkanRenderer.h"

int main(int argc, char* argv[])
{
    MMainModule MainModule;
    MainModule.ModuleStartup();
    
    IRenderer& Renderer = FEngine::Get().GetModule<MVulkanRenderer>().GetRenderer();
    Renderer.Init();

    Renderer.Run();

    Renderer.Shutdown();
    
    MainModule.ModuleShutdown();
    
    return 0;
    
}
