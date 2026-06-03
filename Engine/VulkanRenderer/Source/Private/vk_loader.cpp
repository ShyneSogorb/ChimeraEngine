
#include <expected>
#include <vk_loader.h>

#include <stb_image.h>
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include <type_traits>
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>


#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "Libraries/Memory.h"
#include "Transform.h"
#include "fmt/core.h"
#define STB_IMAGE_IMPLEMENTATION
#include "CoreMinimal.h"
#include "stb_image.h"
#include "SmartPointers/SharedPtr.h"
#include "SmartPointers/SharedRef.h"

using FGOptions = fastgltf::Options;

constexpr bool OverrideColors = false;

VkFilter ExtractFilter(fastgltf::Filter Filter)
{
    using Filters = fastgltf::Filter;
    
    switch (Filter)
    {
        //nearest samplers
        case Filters::Nearest:
        case Filters::NearestMipMapLinear:
        case Filters::NearestMipMapNearest:
            return VK_FILTER_NEAREST;
        
            //linear samplers
        case Filters::Linear:
        case Filters::LinearMipMapLinear:
        case Filters::LinearMipMapNearest:
        default:
            return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter Filter)
{
    using Filters = fastgltf::Filter;
    
    switch (Filter)
    {
        case Filters::NearestMipMapNearest:
        case Filters::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        
        case Filters::NearestMipMapLinear:
        case Filters::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

void FLoadedGltf::Draw(const FMatrix& TopMatrix, FDrawContext& ctx)
{
    //create renderables from scenenodes
    for (const auto& Node : TopNodes)
    {
        Node->Draw(TopMatrix, ctx);
    }
}

void FLoadedGltf::ClearAll()
{
    
    VkDevice Device{Creator->Device};
    
    DescriptorPool.DestroyPool(Device);
    Creator->DestroyBuffer(MaterialDataBuffer);
    
    for (auto& [K, V] : Meshes)
    {
        Creator->DestroyBuffer(V->MeshBuffers.IndexBuffer);
        Creator->DestroyBuffer(V->MeshBuffers.VertexBuffer);
    }
    
    for (auto& [K, V] : Images)
    {
        if (V.Image == Creator->ErrorCheckerboardImage.Image)
        {
            continue; // do not destroy default images
        }    
        Creator->DestroyImage(V);
    }
    
    for (const auto& Sampler : Samplers)
    {
        vkDestroySampler(Device, Sampler, nullptr);
    }
    
}

FOptionalGltfData LoadGltfMeshes(VulkanEngine& Engine, FPath FilePath)
{
    using Opts = fastgltf::Options;
    FilePath = fmt::format("{}{}", RESOURCES_PATH, FilePath.string());
    
    fmt::print("Loading GLTF file: {}\n", FilePath.string());
    
    TSharedPtr<FLoadedGltf> Scene = MakeShared<FLoadedGltf>();
    Scene->Creator = &Engine;
    FLoadedGltf& File = *Scene;
    
    fastgltf::Parser Parser{};
    
    constexpr auto GltfOptions = Opts::DontRequireValidAssetMember | Opts::AllowDouble | Opts::LoadGLBBuffers | Opts::LoadExternalBuffers;
    //Opts::LoadExternalImages;
    
    fastgltf::GltfDataBuffer Data{};
    if (auto DataOpt = fastgltf::GltfDataBuffer::FromPath(FilePath))
    {
        Data = MoveTemp(*MoveTemp(DataOpt));
    }
    
    fastgltf::Asset Gltf;
    
    std::filesystem::path Path = FilePath;
    
    auto Type = fastgltf::determineGltfFileType(Data);
    if (Type == fastgltf::GltfType::glTF)
    {
        auto Load = Parser.loadGltf(Data, Path.parent_path(), GltfOptions);
        if (Load)
        {
            Gltf = MoveTemp(Load.get());
        }else
        {
            std::cerr << "Failed to load GLTF file: " << fastgltf::to_underlying(Load.error()) << std::endl;
            return {};
        }
    } 
    else if (Type == fastgltf::GltfType::GLB)
    {
        auto Load = Parser.loadGltfBinary(Data, Path.parent_path(), GltfOptions);
        if (Load)
        {
            Gltf = MoveTemp(Load.get());
        }else
        {
            std::cerr << "Failed to load GLB file: " << fastgltf::to_underlying(Load.error()) << std::endl;
            return {};
        }
    }
    else
    {
        std::cerr << "Unknown GLTF file type" << std::endl;
        return {};
    }
    
    // we can stimate the descriptors we will need accurately
    TArray<FDescriptorAllocator::FPoolSizeRatio> Sizes{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    
    File.DescriptorPool.Init(Engine.Device, Gltf.materials.size(), Sizes);
    
    //load samplers
    for (fastgltf::Sampler& Sampler : Gltf.samplers)
    {
        VkSamplerCreateInfo SamplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        SamplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        SamplerInfo.minLod = 0;
        
        SamplerInfo.magFilter = ExtractFilter(Sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        SamplerInfo.minFilter = ExtractFilter(Sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        
        SamplerInfo.mipmapMode = ExtractMipmapMode(Sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        
        VkSampler NewSampler;
        vkCreateSampler(Engine.Device, &SamplerInfo, nullptr, &NewSampler);
        
        File.Samplers.Add(NewSampler);
    }
    
    // temporal arrays for all the objects to use while creating the GLTF data
    TArrayShared<FMeshAsset> Meshes;
    TArrayShared<FNode> Nodes;
    TArray<FAllocatedImage> Images;
    TArrayShared<FGltfMaterial> Materials;
    
    //load all textures
    for (fastgltf::Image& Image : Gltf.images)
    {
        TOptional<FAllocatedImage> Img = LoadImage(Engine, Gltf, Image);
        
        if (Img.has_value())
        {
            Images.Add(*Img);
            File.Images[Image.name.c_str()] = *Img;
        }
        else
        {
            // we failed to load, so lets give the slot a default white texture to not completely break loading
            Images.Add(Engine.ErrorCheckerboardImage);
            fmt::println("Gltf failed to load texture {:s}", Image.name);
        }
    }
    
    //Create buffer to hold the material data
    File.MaterialDataBuffer = Engine.CreateBuffer(sizeof(FGltfMetalicRoughness::FMaterialConstants) * Gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
    int32 DataIndex = 0;
    auto SceneMaterialConstants = static_cast<FGltfMetalicRoughness::FMaterialConstants*>(File.MaterialDataBuffer.Info.pMappedData);
    
    for (fastgltf::Material& Mat : Gltf.materials)
    {
        TSharedRef<FGltfMaterial> NewMat = MakeShared<FGltfMaterial>();
        Materials.Add(NewMat);
        File.Materials[Mat.name.c_str()] = NewMat;
        
        FGltfMetalicRoughness::FMaterialConstants Constants;
        
        Constants.ColorFactors = {
            Mat.pbrData.baseColorFactor[0],
            Mat.pbrData.baseColorFactor[1],
            Mat.pbrData.baseColorFactor[2],
            Mat.pbrData.baseColorFactor[3],
        };
        
        Constants.MetalRoughFactors.x = Mat.pbrData.metallicFactor;
        Constants.MetalRoughFactors.y = Mat.pbrData.roughnessFactor;
        
        //write parameters to buffer
        SceneMaterialConstants[DataIndex] = Constants;
        
        EMaterialPass PassType = EMaterialPass::Opaque;
        if (Mat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            PassType = EMaterialPass::Transparent;
        }
        
        FGltfMetalicRoughness::FMaterialResources MaterialResources;
        
        //Default the material textures
        MaterialResources.ColorImage = Engine.WhiteImage;
        MaterialResources.ColorSampler = Engine.DefaultSamplerLinear;
        MaterialResources.MetalRoughImage = Engine.WhiteImage;
        MaterialResources.MetalRoughSampler = Engine.DefaultSamplerLinear;
        
        //Set the uniform buffer for the material data
        MaterialResources.DataBuffer = File.MaterialDataBuffer.Buffer;
        MaterialResources.DataBufferOffset = DataIndex * sizeof(FGltfMetalicRoughness::FMaterialConstants);
        
        //Grab textures from gltf file
        if (Mat.pbrData.baseColorTexture.has_value())
        {
            size_t ImgIndex = Gltf.textures[Mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t SamplerIndex = Gltf.textures[Mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
            
            MaterialResources.ColorImage = Images[ImgIndex];
            MaterialResources.ColorSampler = File.Samplers[SamplerIndex];
        }
        
        //build material
        NewMat->Data = Engine.MetalRoughMat.WriteMaterial(Engine.Device, PassType, MaterialResources, File.DescriptorPool);
        
        DataIndex++;
    }
    
    // use the same vectors for all meshes so that the memory doesnt reallocate as often
    TArray<uint32> Indices;
    TArray<FVertex> Vertices;
    
    for (fastgltf::Mesh& Mesh : Gltf.meshes)
    {
        TSharedRef<FMeshAsset> NewMesh = MakeShared<FMeshAsset>();
        Meshes.Add(NewMesh);
        File.Meshes[Mesh.name.c_str()] = NewMesh;
        NewMesh->Name = Mesh.name;
        
        // clear the mesh arrays each mesh, we dont want to merge them by error
        Indices.Clear();
        Vertices.Clear();
        
        for (auto&& Primitive : Mesh.primitives)
        {
            FGeoSurface NewSurface;
            NewSurface.StartIndex = (uint32)Indices.Num();
            NewSurface.Count = (uint32)Gltf.accessors[Primitive.indicesAccessor.value()].count;
            
            size_t InitialVtx = Vertices.Num();
            
            //load indexes
            {
                fastgltf::Accessor& IndexAccessor = Gltf.accessors[Primitive.indicesAccessor.value()];
                Indices.Reserve(Indices.Num() + IndexAccessor.count);
                
                fastgltf::iterateAccessor<uint32>(Gltf, IndexAccessor, [&](uint32 i)
                {
                   Indices.Add(i + InitialVtx); 
                });
            }
            
            // load vertex positions
            {
                fastgltf::Accessor& PosAccess = Gltf.accessors[Primitive.findAttribute("POSITION")->accessorIndex];
                Vertices.Resize(Vertices.Num() + PosAccess.count);
                
                fastgltf::iterateAccessorWithIndex<FVector3f>(Gltf, PosAccess, [&](FVector3f v, size_t i)
                {
                   FVertex Vertex;
                    Vertex.Position = v;
                    Vertex.Normal = {1, 0, 0};
                    Vertex.Color = FVector4f(1.f);
                    Vertex.UVx = 0;
                    Vertex.UVy = 0;
                    Vertices[InitialVtx + i] = Vertex;
                });
            }
            
            //Load vertex normal
            {
                auto Normals = Primitive.findAttribute("NORMAL");
                if ( Normals != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector3f>(Gltf, Gltf.accessors[(*Normals).accessorIndex],
                        [&](FVector3f v, size_t i)
                        {
                           Vertices[InitialVtx + i].Normal = v;
                        });
                }
            }
            
            //Load UVs
            {
                auto UV = Primitive.findAttribute("TEXCOORD_0");
                if ( UV != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector2f>(Gltf, Gltf.accessors[(*UV).accessorIndex],
                        [&](FVector2f v, size_t i)
                        {
                           Vertices[InitialVtx + i].UVx = v.x;
                           Vertices[InitialVtx + i].UVy = v.y;
                        });
                }
            }
            
            //Load vertex normal
            {
                auto Colors = Primitive.findAttribute("COLOR_0");
                if ( Colors != Primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<FVector4f>(Gltf, Gltf.accessors[(*Colors).accessorIndex],
                        [&](FVector4f v, size_t i)
                        {
                           Vertices[InitialVtx + i].Color = v;
                        });
                }
            }
            
            if (Primitive.materialIndex.has_value())
            {
                NewSurface.Material = Materials[Primitive.materialIndex.value()];
            }
            else{
                NewSurface.Material = Materials[0];
            }
            
            //loop the vertices of this surface, find min/max bounds
            FVector MinPosition = Vertices[InitialVtx].Position;
            FVector MaxPosition = Vertices[InitialVtx].Position;
            for (int32 i = InitialVtx; i < Vertices.Num(); i++)
            {
                MinPosition = glm::min(MinPosition, Vertices[i].Position);
                MaxPosition = glm::max(MaxPosition, Vertices[i].Position);
            }
            
            //Calculate origin and extents from the min/max, lenght as radius
            FBounds& Bounds = NewSurface.Bounds; 
            Bounds.Origin = (MaxPosition + MinPosition) / 2.0f;
            Bounds.Extents = (MaxPosition - MinPosition) / 2.0f;
            Bounds.Radius = glm::length(Bounds.Extents);
            
            NewMesh->Surfaces.Add(NewSurface);
        }
        
        NewMesh->MeshBuffers = Engine.UploadMesh(Indices, Vertices);
        
    }
    
    // load all nodes and their meshes
    for (fastgltf::Node& Node :  Gltf.nodes)
    {
        TSharedPtr<FNode> NewNode;
        
        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (Node.meshIndex.has_value())
        {
            NewNode = MakeShared<FMeshNode>();
            static_cast<FMeshNode*>(NewNode.get())->Mesh = Meshes[*Node.meshIndex];
        } 
        else
        {
            NewNode = MakeShared<FNode>();
        }
        
        Nodes.Add(NewNode);
        File.Nodes[Node.name.c_str()];
        
        std::visit(fastgltf::visitor {
            [&](fastgltf::math::fmat4x4 Matrix)
            {
                FMemory::Memcpy(&NewNode->LocalTransform, Matrix.data(), sizeof(Matrix));
            },
            [&](fastgltf::TRS Transform)
            {
                FVector Translation{
                    Transform.translation[0],
                    Transform.translation[1],
                    Transform.translation[2]
                };
                FQuat Rotation{
                    Transform.rotation[3],
                    Transform.rotation[0],
                    Transform.rotation[1],
                    Transform.rotation[2]
                };
                FVector Scale{
                    Transform.scale[0],
                    Transform.scale[1],
                    Transform.scale[2]
                };
                
                FMatrix TranslationMatrix   { FTransform::Translate(FMatrix(1.0f), Translation) };
                FMatrix RotationMatrix      { FTransform::ToMatrix(Rotation) };
                FMatrix ScaleMatrix         { FTransform::Scale(FMatrix(1.0f), Scale) };
                
                NewNode->LocalTransform = TranslationMatrix * RotationMatrix * ScaleMatrix;
            }
        }, Node.transform);
    }
    
    //run loop again to setup transform hierarchy
    for (int32 i = 0; i < Gltf.nodes.size(); ++i)
    {
        fastgltf::Node& Node = Gltf.nodes[i];
        TSharedPtr<FNode>& SceneNode = Nodes[i];
        
        for (auto& Child : Node.children)
        {
            SceneNode->Children.Add(Nodes[Child]);
            Nodes[Child]->Parent = SceneNode; 
        }
    }
    
    // find the top nodes, with no parents
    for (const auto& Node : Nodes)
    {
        if (Node->Parent.lock() == nullptr)
        {
            File.TopNodes.Add(Node);
            Node->RefreshTransform(FMatrix{1.f});
        }
    }
    
    
    
    return Scene;
}

TOptional<FAllocatedImage> LoadImage(VulkanEngine& Engine, fastgltf::Asset& Asset, fastgltf::Image& Image)
{
    FAllocatedImage NewImage{};
    
    int32 Width, Height, nChannels;
    
    std::visit(fastgltf::visitor {
        [](auto& Arg){fmt::println("monostate");},
            [&](fastgltf::sources::URI& FilePath)
            {
                assert(FilePath.fileByteOffset == 0); //we do not support offsets with stbi
                assert(FilePath.uri.isLocalPath()); // we do not support remote files
                
                const FString Path(FilePath.uri.path().begin(), FilePath.uri.path().end());
                fmt::println("Full uri path is {}", Path);
                
                uint8* Data = stbi_load(Path.c_str(), &Width, &Height, &nChannels, 4);
                if (Data)
                {
                    VkExtent3D ImageSize;
                    ImageSize.width = Width;
                    ImageSize.height = Height;
                    ImageSize.depth = 1;
                    
                    NewImage = Engine.CreateImage(Data, ImageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                    stbi_image_free(Data);
                }
            },
        [&](fastgltf::sources::Vector& Vector)
        {
            fmt::println("Getting path from vector");
            uint8* Data = stbi_load_from_memory((uint8*)Vector.bytes.data(), static_cast<int32>(Vector.bytes.size()), &Width, &Height, &nChannels, 4);
            if (Data)
            {
                VkExtent3D ImageSize;
                ImageSize.width = Width;
                ImageSize.height = Height;
                ImageSize.depth = 1;
                    
                NewImage = Engine.CreateImage(Data, ImageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                stbi_image_free(Data);
            }
        },
        [&](fastgltf::sources::BufferView& View)
        {
            auto& BufferView = Asset.bufferViews[View.bufferViewIndex];
            auto& Buffer = Asset.buffers[BufferView.bufferIndex];
                
            std::visit(fastgltf::visitor {
                [](auto& Arg) {
                    fmt::println("Buffer monostate");
                },
                // ✅ Añade este caso para GLB embebidos
                [&](fastgltf::sources::Array& Array)
                {
                    fmt::println("Getting path from buffer array");
                    uint8* Data = stbi_load_from_memory(
                        (uint8*)Array.bytes.data() + BufferView.byteOffset,
                        static_cast<int32>(BufferView.byteLength),
                        &Width, &Height, &nChannels, 4);
                    if (Data)
                    {
                        VkExtent3D ImageSize;
                        ImageSize.width = Width;
                        ImageSize.height = Height;
                        ImageSize.depth = 1;
                        NewImage = Engine.CreateImage(Data, ImageSize, 
                            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                        stbi_image_free(Data);
                    }
                },
                [&](fastgltf::sources::Vector& Vector)
                {
                    fmt::println("Getting path from buffer view");
                    uint8* Data = stbi_load_from_memory(
                        (uint8*)Vector.bytes.data() + BufferView.byteOffset,
                        static_cast<int32>(BufferView.byteLength),
                        &Width, &Height, &nChannels, 4);
                    if (Data)
                    {
                        VkExtent3D ImageSize;
                        ImageSize.width = Width;
                        ImageSize.height = Height;
                        ImageSize.depth = 1;
                        NewImage = Engine.CreateImage(Data, ImageSize,
                            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
                        stbi_image_free(Data);
                    }
                }
            }, Buffer.data);
        }
    }, Image.data);
    
    // if any of the attempts to load the data failed, we have'nt written the image
    // so handle is null
    if (NewImage.Image == VK_NULL_HANDLE)
        return {};
    else return NewImage;
}
