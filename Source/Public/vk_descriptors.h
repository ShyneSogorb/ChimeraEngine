#pragma once

#include <vk_types.h>

struct FDescriptorLayoutBuilder
{
    TArray<VkDescriptorSetLayoutBinding> Bindings;

    void AddBinding(uint32_t Binding, VkDescriptorType Type);
    void Clear();
    VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages,
                                VkDescriptorSetLayoutCreateFlags Flags = 0, void* pNext = nullptr);
};

struct FDescriptorAllocator
{
    struct FPoolSizeRation
    {
        VkDescriptorType Type;
        float Ratio;
    };

    void InitPool(VkDevice Device, uint32_t MaxSets, TArrayView<FPoolSizeRation> PoolRatios);
    void ClearPools(VkDevice Device);
    void DestroyPool(VkDevice Device);

    VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout);

private:
    VkDescriptorPool GetPool(VkDevice Device);
    VkDescriptorPool CreatePool(VkDevice Device, uint32 SetCount, TArrayView<FPoolSizeRation> PoolRatios);

    TArray<FPoolSizeRation> Ratios;
    TArray<VkDescriptorPool> FullPools;
    TArray<VkDescriptorPool> ReadyPools;

    uint32 SetsPerPool;
};

struct FDescriptorWriter
{
    TDeque<VkDescriptorImageInfo> ImageInfos;
    TDeque<VkDescriptorBufferInfo> BufferInfos;
    TArray<VkWriteDescriptorSet> Writes;

    void WriteImage(int Binding, VkImageView Image, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type);
    void WriteBuffer(int Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type);

    void Clear();
    void UpdateSet(VkDevice Device, VkDescriptorSet Set);
};

