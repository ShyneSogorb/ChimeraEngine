#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

using FPath = std::filesystem::path;

struct FGeoSurface
{
    uint32 StartIndex;
    uint32 Count;
};

struct FMeshAsset
{
    FString Name;
    
    TArray<FGeoSurface> Surfaces;
    FGpuMeshBuffers MeshBuffers;
};

//forward declaration
class VulkanEngine;

namespace vkLoader
{
    TOptional<TArray<TSharedPtr<FMeshAsset>>> LoadGltfMeshes(VulkanEngine& Engine, FPath FilePath);
}