// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <expected>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

template <typename T> 
using TArray = std::vector<T>;

template <typename T>
using TOptional = std::optional<T>;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T, size_t N>
using TStaticArray = std::array<T, N>;

template <typename T, typename ErrorType>
using TExpected = std::expected<T, ErrorType>;

using FVector4 = glm::vec4;
using FVector3 = glm::vec3;
using FVector = FVector3;

#define FORWARD_VECTOR { 1.f, 0.f, 0.f}

using FMatrix4 = glm::mat4;
using FMatrix = FMatrix4;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using FString = std::string;

namespace FMemory
{
    inline void* Memcpy(void* Dst, void const* Src, size_t Size)
    {
        return memcpy(Dst, Src, Size);
    }
}

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

struct FDeletionQueue
{
    void PushFunction(std::function<void()>&& Function)
    {
        Deletors.push_back(Function);
    }
	
    void Flush()
    {
        for (auto It = Deletors.rbegin(); It != Deletors.rend(); ++It)
        {
            (*It)();
        }
		
        Deletors.clear();
    }
	
private:
    std::deque<std::function<void()>> Deletors;
};


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
    FVector3 Position;
    float UVx;
    FVector3 Normal;
    float UVy;
    FVector4 Color;
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