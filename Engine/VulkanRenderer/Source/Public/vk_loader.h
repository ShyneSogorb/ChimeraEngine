#pragma once

#include <vk_types.h>

#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>

#include "Containers/Map.h"
#include "Containers/Optional.h"
#include "Types/String.h"

using FPath = std::filesystem::path;

struct FGltfMaterial
{
    FMaterialInstance Data;
};

namespace fastgltf
{
    class Asset;
    struct Image;
}

//forward declaration
class VulkanEngine;

struct FLoadedGltf : public IRenderable
{
    //storage fpr all the data on a gives gltf file
    TMap<FString, TSharedPtr<struct FMeshAsset>> Meshes;
    TMap<FString, TSharedPtr<FNode>> Nodes;
    TMap<FString, FAllocatedImage> Images;
    TMap<FString, TSharedPtr<FGltfMaterial>> Materials;
    
    //nodes that dont have parent, for iterationg through the file in tree order
    TArray<TSharedPtr<FNode>> TopNodes;
    
    TArray<VkSampler> Samplers;
    
    FDescriptorAllocator DescriptorPool;
    
    FAllocatedBuffer MaterialDataBuffer;
    
    VulkanEngine* Creator;
    
    ~FLoadedGltf() { ClearAll(); };
    
    virtual void Draw(const FMatrix& TopMatrix, FDrawContext& ctx) override;
    
private:
    
    void ClearAll();
    
};

using FOptionalGltfData = TOptional<TSharedPtr<FLoadedGltf>>; 

FOptionalGltfData LoadGltfMeshes(VulkanEngine& Engine, FPath FilePath);

TOptional<FAllocatedImage> LoadImage(VulkanEngine& Engine, fastgltf::Asset& Asset, fastgltf::Image& Image);
