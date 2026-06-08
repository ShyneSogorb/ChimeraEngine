
#include <vk_pipelines.h>

#include <filesystem>
#include <fstream>
#include <vk_initializers.h>
#include <fmt/core.h>

#include "Containers/StaticArray.h"

bool vkutil::LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule& OutShaderModule, FString* BuildPath)
{
    
    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    using SpirvType = uint32_t;
    
    FString Path = fmt::format("{:s}{:s}", SHADER_PATH, FilePath);
    if (BuildPath) *BuildPath = Path;
    if (!std::filesystem::exists(Path))
    {
        fmt::print("Shader file not found: {}\n", Path);
        return false;
    }
    
    //Open file
    std::ifstream File(Path, std::ios::ate | std::ios::binary);
    
    
    if (!File.is_open())
    {
        fmt::print("Failed to open shader file (already open): {}\n", Path);
        return false;
    }
    
    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t FileSize = File.tellg();
    
    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // array big enough for the entire file
    TArray<SpirvType> Buffer{};
    Buffer.Resize(FileSize / sizeof(SpirvType));
    
    //put file cursor at beginning
    File.seekg(0);
    
    //Load entire file to buffer
    File.read((char*)Buffer.GetData(), FileSize);
    
    //With the file loaded we can close it
    File.close();
    
    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo CreateInfo{};
    CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    CreateInfo.pNext = nullptr;
    
    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    CreateInfo.codeSize = Buffer.Num() * sizeof(SpirvType);
    CreateInfo.pCode = Buffer.GetData();
    
    //Check that the creation goes well
    if (vkCreateShaderModule(Device, &CreateInfo, nullptr, &OutShaderModule) == VK_SUCCESS)
    {
        return true;
    }
    else
    {
        //try deduce what went wrong
        if (CreateInfo.codeSize == 0)
        {
            fmt::print("Failed to create shader module: code size is 0, likely the file was empty or not read correctly: {}\n", Path);
        }
        else if (CreateInfo.pCode == nullptr)        {
            fmt::print("Failed to create shader module: code pointer is null, likely the file was not read correctly: {}\n", Path);
        }
        else {
            fmt::print("Failed to create shader module for unknown reason: {}\n", Path);
        }
        return false;
    }
}

void FPipelineBuilder::Clear()
{
    // clear all of the structs we need back to 0 with their correct stype
    
    InputAssembly           = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO    };
    Rasterizer              = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO     };
    ColorBlendAttachment    = {};
    Multisampling           = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO       };
    Layout                     = {};
    DepthStencil            = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO     };
    RenderInfo               = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR           };
    ShaderStages.Clear();
}

VkPipeline FPipelineBuilder::BuildPipeline(VkDevice Device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo ViewportState{};
    ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportState.pNext = nullptr;
    
    ViewportState.viewportCount = 1;
    ViewportState.scissorCount = 1;
    
    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo ColorBlending{};
    ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlending.pNext = nullptr;
    
    ColorBlending.logicOpEnable = VK_FALSE;
    ColorBlending.logicOp = VK_LOGIC_OP_COPY;
    ColorBlending.attachmentCount = 1;
    ColorBlending.pAttachments = &ColorBlendAttachment;
    
    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo VertexInputInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    
    // build the actual pipeline
    // we now use all of the info structs we have been writing into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo PipelineInfo { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    PipelineInfo.pNext = &RenderInfo;
    
    PipelineInfo.stageCount = (uint32_t)ShaderStages.Num();
    PipelineInfo.pStages = ShaderStages.GetData();
    PipelineInfo.pVertexInputState = &VertexInputInfo;
    PipelineInfo.pInputAssemblyState = &InputAssembly;
    PipelineInfo.pViewportState = &ViewportState;
    PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &Multisampling;
    PipelineInfo.pColorBlendState = &ColorBlending;
    PipelineInfo.pDepthStencilState = &DepthStencil;
    PipelineInfo.layout = Layout;
    
    TStaticArray<VkDynamicState, 2> State = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo DynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    DynamicInfo.pDynamicStates = &State[0];
    DynamicInfo.dynamicStateCount = 2;
    
    PipelineInfo.pDynamicState = &DynamicInfo;
    
    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline NewPipeline;
    if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &NewPipeline) != VK_SUCCESS)
    {
        fmt::println("Failed to create pipeline");
        return VK_NULL_HANDLE;
    }else
    {
        return NewPipeline;
    }
}

void FPipelineBuilder::SetShaders(VkShaderModule VertexShader, VkShaderModule FragmentShader)
{
    ShaderStages.Clear();
    
    ShaderStages.Add(
        Vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, VertexShader)
    );
    
    ShaderStages.Add(
        Vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, FragmentShader)
    );
}

void FPipelineBuilder::SetInputTopology(VkPrimitiveTopology Topology)
{
    InputAssembly.topology = Topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    InputAssembly.primitiveRestartEnable = VK_FALSE;
}

void FPipelineBuilder::SetPoligonMode(VkPolygonMode Mode)
{
    Rasterizer.polygonMode = Mode;
    Rasterizer.lineWidth = 1.f;
}

void FPipelineBuilder::SetCullMode(VkCullModeFlags CullMode, VkFrontFace FrontFace)
{
    Rasterizer.cullMode = CullMode;
    Rasterizer.frontFace = FrontFace;
}

void FPipelineBuilder::SetMultisamplingNone()
{
    Multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    Multisampling.minSampleShading = 1.f;
    Multisampling.pSampleMask = nullptr;
    //no alpha coverage either
    Multisampling.alphaToCoverageEnable = VK_FALSE;
    Multisampling.alphaToOneEnable = VK_FALSE;
}

void FPipelineBuilder::DisableBlending()
{
    //Default write mask
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    //no blending
    ColorBlendAttachment.blendEnable = VK_FALSE;
}

void FPipelineBuilder::EnableBlendingAdditive()
{
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_TRUE;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void FPipelineBuilder::EnableBlendingAlphaBlend()
{
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_TRUE;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void FPipelineBuilder::SetColorAttachmentFormat(VkFormat Format)
{
    ColorAttachmentFormat = Format;
    //Connect the format to the renderInfo structure
    RenderInfo.colorAttachmentCount = 1;
    RenderInfo.pColorAttachmentFormats = &ColorAttachmentFormat;
}

void FPipelineBuilder::SetDepthFormat(VkFormat Format)
{
    RenderInfo.depthAttachmentFormat = Format;
}

void FPipelineBuilder::DisableDepthTest()
{
    DepthStencil.depthTestEnable = VK_FALSE;
    DepthStencil.depthWriteEnable = VK_FALSE;
    DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    DepthStencil.depthBoundsTestEnable = VK_FALSE;
    DepthStencil.stencilTestEnable = VK_FALSE;
    DepthStencil.front = {};
    DepthStencil.back = {};
    DepthStencil.minDepthBounds = 0.f;
    DepthStencil.maxDepthBounds = 1.f;
}

void FPipelineBuilder::EnableDepthTest(bool bDepthWriteEnable, VkCompareOp Op)
{
    DepthStencil.depthTestEnable = VK_TRUE;
    DepthStencil.depthWriteEnable = bDepthWriteEnable;
    DepthStencil.depthCompareOp = Op;
    DepthStencil.depthBoundsTestEnable = VK_FALSE;
    DepthStencil.stencilTestEnable = VK_FALSE;
    DepthStencil.front = {};
    DepthStencil.back = {};
    DepthStencil.minDepthBounds = 0.f;
    DepthStencil.maxDepthBounds = 1.f;
}
