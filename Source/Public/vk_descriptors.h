#pragma once

#include <vk_types.h>

struct FDescriptorLayoutBuilder
{
    TArray<VkDescriptorSetLayoutBinding> Bindings;
    
    void AddBinding(uint32_t Binding, VkDescriptorType Type);
    void Clear();
    VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags Flags = 0);
    
};

struct FDescriptorAllocator
{
    struct FPoolSizeRation
    {
        VkDescriptorType Type;
        float Ratio;
    };
    
    VkDescriptorPool Pool{};
    
    void InitPool(VkDevice Device, uint32_t MaxSets, std::span<FPoolSizeRation> PoolRatios);
    void ClearDescriptors(VkDevice Device);
    void DestroyPool(VkDevice Device);
    
    VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout);
};