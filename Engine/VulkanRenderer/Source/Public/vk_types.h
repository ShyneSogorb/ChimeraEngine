// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once


#include <vk_mem_alloc.h>

#include "Libraries/MathLibrary.h"
#include "vulkan/vulkan.h"
#include <vulkan/vk_enum_string_helper.h>
#include "Types/Types.h"
#include "Containers/Array.h"
#include "SmartPointers/SharedPtr.h"
#include "SmartPointers/UniquePtr.h"
#include "SmartPointers/WeakPtr.h"

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)


struct FAllocatedImage
{
    VkImage Image;
    VkImageView ImageView;
    VmaAllocation Allocation;
    VkExtent3D ImageExtent;
    VkFormat ImageFormat;
};

struct FAllocatedBuffer
{
    VkBuffer Buffer;
    VmaAllocation Allocation;
    VmaAllocationInfo Info;
};

struct FVertex
{
    FVector3f Position;
    float UVx;
    FVector3f Normal;
    float UVy;
    FVector4f Color;
};

//holds the resources needed for a mesh
struct FGpuMeshBuffers
{
    FAllocatedBuffer IndexBuffer;
    FAllocatedBuffer VertexBuffer;
    VkDeviceAddress VertexBufferAddress;
};

//push constants for our mesh object draws
struct FGpuDrawPushConstants
{
    FMatrix WorldMatrix;
    VkDeviceAddress VertexBuffer;
};

struct FGpuSceneData
{
    FMatrix View;
    FMatrix Projection;
    FMatrix ViewProjection;
    FColor AmbientColor;
    FVector4 SunlightDirection; //w for intensity
    FColor SunlightColor;
};

enum class EMaterialPass : uint8
{
    Opaque,
    Transparent,
    Other
};

struct FMaterialPipeline
{
    VkPipeline Pipeline;
    VkPipelineLayout Layout;
};

struct FMaterialInstance
{
    FMaterialPipeline* Pipeline;
    VkDescriptorSet MaterialSet;
    EMaterialPass PassType;
};

struct FDrawContext;

//base class for a renderable dynamic object
class IRenderable
{
    virtual void Draw(const FMatrix& TopMatrix, FDrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct FNode : public IRenderable
{
    //Parent pointer must be weak to avoid circular dependency
    TWeakPtr<FNode> Parent;
    TArray<TSharedPtr<FNode>> Children;
    
    FMatrix LocalTransform{1.f};
    FMatrix WorldTransform{1.f};
    
    void RefreshTransform(const FMatrix& ParentMatrix)
    {
        WorldTransform = ParentMatrix * LocalTransform;
        for (auto& Child : Children)
        {
            Child->RefreshTransform(WorldTransform);
        }
    }
    
    virtual void Draw(const FMatrix& TopMatrix, FDrawContext& ctx) override
    {
        //Draw children
        for (auto& Child : Children)
        {
            Child->Draw(TopMatrix, ctx);
        }
    }
};