
#include <expected>
#include <vk_loader.h>

#include <stb_image.h>
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>


#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "Memory.h"

using FGOptions = fastgltf::Options;

constexpr bool OverrideColors = false;


TOptional<TArray<TSharedPtr<FMeshAsset>>> vkLoader::LoadGltfMeshes(VulkanEngine& Engine, FPath FilePath)
{
    FilePath = fmt::format("{}{}", RESOURCES_PATH, FilePath.string());
    
    std::cout << "Loading GLTF: " << FilePath << std::endl;
    
    auto DataExp = fastgltf::GltfDataBuffer::FromPath(FilePath);
    if (!DataExp) {
        std::cout << "Failed to load GLTF file: " << FilePath << std::endl;
        return {};
    }
    
    fastgltf::GltfDataBuffer& Data = DataExp.get();
    
    constexpr auto GltfOpts = FGOptions::LoadGLBBuffers | FGOptions::LoadExternalBuffers;
    
    fastgltf::Asset Asset{};
    fastgltf::Parser Parser{};
    
    auto Load = Parser.loadGltfBinary(Data, FilePath.parent_path(), GltfOpts);
    if (Load)
    {
        Asset = MoveTemp(Load.get());
        fmt::print("Successfully loaded asset {}\n", FilePath.string());
    }
    else
    {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(Load.error()));
        return {};
    }
    
    TArray<TSharedPtr<FMeshAsset>> Meshes{};
    
    //use the same vectors for all meshes so that the memory doesnt reallocate as often
    TArray<uint32> Indices{};
    TArray<FVertex> Vertices{};
    
    for (fastgltf::Mesh& Mesh : Asset.meshes)
    {
        FMeshAsset NewMesh{};
        
        NewMesh.Name = Mesh.name;
        
        //Clear the mesh arrays each mesh to avoid merging by error
        Indices.clear();
        Vertices.clear();

        for (auto&& Primitive : Mesh.primitives)
        {
            FGeoSurface NewSurface{};
            NewSurface.StartIndex = (uint32)Indices.size();
            NewSurface.Count = (uint32)Asset.accessors[Primitive.indicesAccessor.value()].count;
            
            size_t InitialVertex = Vertices.size();
            
            //Load indexes
            {
                fastgltf::Accessor& IndexAccessor = Asset.accessors[Primitive.indicesAccessor.value()];
                Indices.reserve(Indices.size() + IndexAccessor.count);
                
                fastgltf::iterateAccessor<uint32>(Asset, IndexAccessor, [&](uint32 idx)
                {
                    Indices.push_back(idx + InitialVertex);
                });
            }
            
            //Load vertex positions
            {
                fastgltf::Accessor& PosAccessor = Asset.accessors[Primitive.findAttribute("POSITION")->accessorIndex];
                Vertices.resize(Vertices.size() + PosAccessor.count);
                
                //@warning FVector3f (vector float) instad of FVector since FVector can be float or double
                fastgltf::iterateAccessorWithIndex<FVector3f>(Asset, PosAccessor,
                    [&](FVector3f Position, size_t Index)
                    {
                        FVertex NewVtx{};
                        NewVtx.Position = Position;
                        NewVtx.Normal = FORWARD_VECTOR;
                        NewVtx.Color = FVector4{1};
                        NewVtx.UVx = 0;
                        NewVtx.UVy = 0;
                        Vertices[InitialVertex + Index] = NewVtx;
                    }
                );
            }
            
            //Load vertex normal
            {
                auto Normals = Primitive.findAttribute("NORMAL");
                if (Normals != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector3f>(Asset, Asset.accessors[Normals->accessorIndex],
                        [&](FVector3f Normal, size_t Index)
                        {
                            Vertices[InitialVertex + Index].Normal = Normal;
                        } 
                    );
                }
            }
            
            //Load UVs
            {
                auto Uv = Primitive.findAttribute("TEXCOORD_0");
                if (Uv != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector2f>(Asset, Asset.accessors[Uv->accessorIndex],
                        [&](FVector2f UV, size_t Index)
                        {
                            Vertices[InitialVertex + Index].UVx = UV.x;
                            Vertices[InitialVertex + Index].UVy = UV.y;
                        }
                    );
                }
            }
            
            //load vertex colors
            {
                auto Colors = Primitive.findAttribute("COLOR_0");
                if (Colors != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector4f>(Asset, Asset.accessors[Colors->accessorIndex],
                        [&](FVector4f Color, size_t Index)
                        {
                            Vertices[InitialVertex + Index].Color = Color;
                        }
                    );
                }
            }
            
            NewMesh.Surfaces.push_back(NewSurface);
        }
        
        //Display vertex normals
        if constexpr (OverrideColors)
        {
            for (FVertex& vtx : Vertices)
                vtx.Color = FVector4f{vtx.Normal, 1.f};
        }
        
        NewMesh.MeshBuffers = Engine.UploadMesh(Indices, Vertices);
        
        Meshes.emplace_back(MakeShared<FMeshAsset>(MoveTemp(NewMesh)));
    }

    return Meshes;
    
}
