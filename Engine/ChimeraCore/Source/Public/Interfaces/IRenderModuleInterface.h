
#pragma once
#include "IModuleInterface.h"
#include "Macros/CompilerAbstractions.h"



struct IRenderModuleInterface : public IModuleInterface
{
    
    virtual void ModuleStartup(){};
    virtual void ModuleShutdown(){};
    
    NO_DISCARD virtual class IRenderer& GetRenderer() const = 0;
    
    virtual ~IRenderModuleInterface() = default;
    
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // ─── Ciclo de vida ───────────────────────────────────────────
    //virtual bool Init(SDL_Window* window, int width, int height) = 0;
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Run() = 0;

    // ─── Frame ───────────────────────────────────────────────────
    //virtual void BeginFrame() = 0;
    //virtual void EndFrame()   = 0;
    virtual bool IsRunning()  = 0;

    // ─── Recursos ────────────────────────────────────────────────
    // virtual MeshHandle    UploadMesh(const std::vector<Vertex>& vertices,
    //                                  const std::vector<uint32_t>& indices) = 0;
    // virtual TextureHandle UploadTexture(const char* path)                  = 0;
    // virtual void          DestroyMesh(MeshHandle handle)                   = 0;
    // virtual void          DestroyTexture(TextureHandle handle)             = 0;

    // ─── Dibujo ──────────────────────────────────────────────────
    // virtual void DrawMesh(MeshHandle mesh,
    //                       TextureHandle texture,
    //                       const Transform& transform) = 0;
    //
    // virtual void SetCamera(const glm::vec3& position,
    //                        const glm::vec3& target,
    //                        float fovDegrees) = 0;

    // ─── Factory ─────────────────────────────────────────────────
    //static std::unique_ptr<IRenderer> Create();
};