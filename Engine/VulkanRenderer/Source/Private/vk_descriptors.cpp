

#include "vk_descriptors.h"
#include "fmt/core.h"

constexpr uint32 MaxSetsPerPool = 4092; 

void FDescriptorLayoutBuilder::AddBinding(uint32_t Binding, VkDescriptorType Type)
{
    VkDescriptorSetLayoutBinding NewBind{};
    NewBind.binding = Binding;
    NewBind.descriptorCount = 1;
    NewBind.descriptorType = Type;
    
    Bindings.Add(NewBind);
}

void FDescriptorLayoutBuilder::Clear()
{
    Bindings.Clear();
}


VkDescriptorSetLayout FDescriptorLayoutBuilder::Build(VkDevice Device, VkShaderStageFlags ShaderStages, VkDescriptorSetLayoutCreateFlags Flags, void* pNext)
{
    for (auto& Bind : Bindings)
    {
        Bind.stageFlags |= ShaderStages;
    }
    
    VkDescriptorSetLayoutCreateInfo Info {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    Info.pNext = pNext;
    
    Info.pBindings = Bindings.GetData();
    Info.bindingCount =  (uint32_t)Bindings.Num();
    Info.flags = Flags;
    
    VkDescriptorSetLayout Set;
    VK_CHECK(vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Set));
    
    return Set;
    
}

void FDescriptorAllocator::Init(VkDevice Device, uint32_t MaxSets, TArrayView<FPoolSizeRatio> PoolRatios)
{
    Ratios.Clear();
    Ratios.Reserve(PoolRatios.Num());
    
    for (auto& Ratio : PoolRatios) {
        Ratios.Add(Ratio);
    }
    
    VkDescriptorPool NewPool = CreatePool(Device, MaxSets, PoolRatios);
    
    SetsPerPool = FMath::Min<uint32>((uint32)(MaxSets * 1.5), MaxSetsPerPool); //grown for next allocations
    
    ReadyPools.Add(NewPool);
}



void FDescriptorAllocator::ClearPools(VkDevice Device)
{
    for (VkDescriptorPool Pool : ReadyPools) {
        vkResetDescriptorPool(Device, Pool, 0);
    }
    for (VkDescriptorPool Pool : FullPools) {
        vkResetDescriptorPool(Device, Pool, 0);
        ReadyPools.Add(Pool);
    }
    FullPools.Clear();
}

void FDescriptorAllocator::DestroyPool(VkDevice Device)
{
    for (auto Pool : ReadyPools)
    {
        vkDestroyDescriptorPool(Device, Pool, nullptr);
    }
    for (auto Pool : FullPools)
    {
        vkDestroyDescriptorPool(Device, Pool, nullptr);
    }
    FullPools.Clear();
}

VkDescriptorSet FDescriptorAllocator::Allocate(VkDevice Device, VkDescriptorSetLayout Layout)
{

    //Get or create a pool to allocate from
    VkDescriptorPool Pool = GetPool(Device);
    
    VkDescriptorSetAllocateInfo AllocInfo {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    AllocInfo.pNext = nullptr;
    AllocInfo.descriptorPool = Pool;
    AllocInfo.descriptorSetCount = 1;
    AllocInfo.pSetLayouts = &Layout;
    
    VkDescriptorSet DescSet;
    VkResult Result = vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet);
    
    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        FullPools.Add(Pool);
        
        Pool = GetPool(Device);
        AllocInfo.descriptorPool = Pool;
        
        VK_CHECK(vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet));
    }
    
    ReadyPools.Add(Pool);
    return DescSet;
}

VkDescriptorPool FDescriptorAllocator::GetPool(VkDevice Device)
{
    VkDescriptorPool NewPool;
    if (ReadyPools.Num() > 0){
        NewPool = ReadyPools.PopElement();
    }
    else {
        //Need to create a new pool
        NewPool = CreatePool(Device, SetsPerPool, Ratios);
        
        SetsPerPool = FMath::Min<uint32>(uint32(SetsPerPool * 1.5), MaxSetsPerPool);
    }
    
    return NewPool;
}

VkDescriptorPool FDescriptorAllocator::CreatePool(VkDevice Device, uint32 SetCount,
    TArrayView<FPoolSizeRatio> PoolRatios)
{
    TArray<VkDescriptorPoolSize> PoolSizes;
    for (const FPoolSizeRatio& Ratio : PoolRatios) {
        PoolSizes.Add(VkDescriptorPoolSize{
            .type = Ratio.Type,
            .descriptorCount = uint32(Ratio.Ratio * SetCount)
        });
    }
    
    VkDescriptorPoolCreateInfo PoolInfo {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    PoolInfo.flags = 0;
    PoolInfo.maxSets = SetCount;
    PoolInfo.poolSizeCount = (uint32)PoolSizes.Num();
    PoolInfo.pPoolSizes = PoolSizes.GetData();
    
    VkDescriptorPool NewPool;
    vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &NewPool);
    return NewPool;
}

void FDescriptorWriter::WriteImage(int Binding, VkImageView Image, VkSampler Sampler, VkImageLayout Layout,
    VkDescriptorType Type)
{
    VkDescriptorImageInfo& Info = ImageInfos.Emplace(VkDescriptorImageInfo{
        .sampler = Sampler,
        .imageView = Image,
        .imageLayout = Layout,
    });
    
    VkWriteDescriptorSet Write {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    
    Write.dstBinding = Binding;
    Write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    Write.descriptorCount = 1;
    Write.descriptorType = Type;
    Write.pImageInfo = &Info;
    
    Writes.Add(Write);
}

void FDescriptorWriter::WriteBuffer(int Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type)
{
    VkDescriptorBufferInfo& Info = BufferInfos.Emplace(VkDescriptorBufferInfo{
        .buffer = Buffer,
        .offset = Offset,
        .range = Size,
    });
    
    VkWriteDescriptorSet Write {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    
    Write.dstBinding = Binding;
    Write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    Write.descriptorCount = 1;
    Write.descriptorType = Type;
    Write.pBufferInfo = &Info;
    
    Writes.Add(Write);
}

void FDescriptorWriter::Clear()
{
    ImageInfos.Clear();
    Writes.Clear();
    BufferInfos.Clear();
}

void FDescriptorWriter::UpdateSet(VkDevice Device, VkDescriptorSet Set)
{
    for (VkWriteDescriptorSet& Write : Writes)
    {
        Write.dstSet = Set;
    }
    
    vkUpdateDescriptorSets(Device, (uint32)Writes.Num(), Writes.GetData(), 0, nullptr);
}

