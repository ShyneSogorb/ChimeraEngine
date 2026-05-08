#include <vk_descriptors.h>

void FDescriptorLayoutBuilder::AddBinding(uint32_t Binding, VkDescriptorType Type)
{
    VkDescriptorSetLayoutBinding NewBind{};
    NewBind.binding = Binding;
    NewBind.descriptorCount = 1;
    NewBind.descriptorType = Type;
    
    Bindings.push_back(NewBind);
}

void FDescriptorLayoutBuilder::Clear()
{
    Bindings.clear();
}


VkDescriptorSetLayout FDescriptorLayoutBuilder::Build(VkDevice Device, VkShaderStageFlags ShaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags Flags)
{
    for (auto& Bind : Bindings)
    {
        Bind.stageFlags |= ShaderStages;
    }
    
    VkDescriptorSetLayoutCreateInfo Info {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    Info.pNext = pNext;
    
    Info.pBindings = Bindings.data();
    Info.bindingCount =  (uint32_t)Bindings.size();
    Info.flags = Flags;
    
    VkDescriptorSetLayout Set;
    VK_CHECK(vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Set));
    
    return Set;
    
}

void FDescriptorAllocator::InitPool(VkDevice Device, uint32_t MaxSets, std::span<FPoolSizeRation> PoolRatios)
{
    TArray<VkDescriptorPoolSize> PoolSizes;
    for (FPoolSizeRation Ratio : PoolRatios)
    {
        PoolSizes.push_back(VkDescriptorPoolSize{
            .type = Ratio.Type,
            .descriptorCount = uint32_t(Ratio.Ratio * MaxSets)
        });
    }
    
    VkDescriptorPoolCreateInfo PoolInfo {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    PoolInfo.flags = 0;
    PoolInfo.maxSets = MaxSets;
    PoolInfo.poolSizeCount = (uint32_t)PoolSizes.size();
    PoolInfo.pPoolSizes = PoolSizes.data();
    
    vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool);
}

void FDescriptorAllocator::ClearDescriptors(VkDevice Device)
{
    vkResetDescriptorPool(Device, Pool, 0);
}

void FDescriptorAllocator::DestroyPool(VkDevice Device)
{
    vkDestroyDescriptorPool(Device, Pool, nullptr);
}

VkDescriptorSet FDescriptorAllocator::Allocate(VkDevice Device, VkDescriptorSetLayout Layout)
{
    VkDescriptorSetAllocateInfo AllocInfo {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    AllocInfo.pNext = nullptr;
    AllocInfo.descriptorPool = Pool;
    AllocInfo.descriptorSetCount = 1;
    AllocInfo.pSetLayouts = &Layout;
    
    VkDescriptorSet DescSet;
    VK_CHECK(vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet));
    
    return DescSet;
}
