#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

#include "vk_descriptors.h"
#include "fastgltf/dxmath_element_traits.hpp"

using FPath = std::filesystem::path;

struct FGltfMaterial
{
    FMaterialInstance Data;
};

struct FGeoSurface
{
    uint32 StartIndex;
    uint32 Count;
    TSharedPtr<FGltfMaterial> Material;
};

struct FMeshAsset
{
    FString Name;
    
    TArray<FGeoSurface> Surfaces;
    FGpuMeshBuffers MeshBuffers;
};

//forward declaration
class VulkanEngine;

struct FLoadedGltf : public IRenderable
{
    //storage fpr all the data on a gives gltf file
    TMap<FString, TSharedPtr<FMeshAsset>> Meshes;
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
