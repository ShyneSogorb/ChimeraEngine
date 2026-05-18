#pragma once 
#include <vk_types.h>

namespace vkutil {

    bool LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule& OutShaderModule);
    
};

struct FPipelineBuilder
{
    TArray<VkPipelineShaderStageCreateInfo> ShaderStages;
    
    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkPipelineColorBlendAttachmentState ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineLayout Layout;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;
    VkFormat ColorAttachmentFormat;
    
    FPipelineBuilder() { Clear(); }
    void Clear();
    
    VkPipeline BuildPipeline(VkDevice Device);
    
    void SetShaders(VkShaderModule VertexShader, VkShaderModule FragmentShader);
    void SetInputTopology(VkPrimitiveTopology Topology);
    void SetPoligonMode(VkPolygonMode Mode);
    void SetCullMode(VkCullModeFlags CullMode, VkFrontFace FrontFace);
    void SetMultisamplingNone();
    void DisableBlending();
    void EnableBlendingAdditive();
    void EnableBlendingAlphaBlend();
    void SetColorAttachmentFormat(VkFormat Format);
    void SetDepthFormat(VkFormat Format);
    void DisableDepthTest();
    void EnableDepthTest(bool bDepthWriteEnable, VkCompareOp Op);
};