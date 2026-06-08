
#include "Engine.h"
#include "Interfaces/IRenderer.h"
#include "MainModule.h"
#include "Interfaces/IInputHandleModuleInterface.h"
#include "Interfaces/IRenderModuleInterface.h"

int main(int argc, char* argv[])
{
    MMainModule MainModule;
    MainModule.ModuleStartup();
    
    IRenderer& Renderer = FEngine::Get().GetModule<IRenderModuleInterface>().GetRenderer();
    Renderer.Init();
    
    IInputHandler& InputHandler = FEngine::Get().GetModule<IInputHandleModuleInterface>().GetInputHandler();
    
    while (!InputHandler.RequestQuit())
    {
        Renderer.Run();
        InputHandler.UpdateInputs();
    }

    Renderer.Shutdown();
    
    MainModule.ModuleShutdown();
    
    return 0;
    
}
