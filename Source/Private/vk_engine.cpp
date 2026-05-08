//> includes
#include "vk_engine.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_images.h>
#include <VkBootstrap.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "vk_loader.h"

#include "vk_pipelines.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtx/transform.hpp"

#define DRAW_TRIANGLE 0

VulkanEngine* loadedEngine = nullptr;

constexpr bool bUseValidationLayers = true; 
constexpr bool bVSync = true;

consteval uint64_t McsToNs(uint64_t Mcs)    { return Mcs * 1000; }
consteval uint64_t MsToMcs(uint64_t Ms)     { return Ms * 1000; }
consteval uint64_t SToMs(uint64_t S)        { return S * 1000; }

consteval uint64_t MsToNs(uint64_t Ms)      { return McsToNs(MsToMcs(Ms)); }

consteval uint64_t SToMcs(uint64_t S)       { return MsToMcs(SToMs(S)); }
consteval uint64_t SToNs(uint64_t S)        { return McsToNs(SToMcs(S)); }


VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    Window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WindowExtent.width,
        WindowExtent.height,
        window_flags);

    
    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();
    InitDescriptors();
    
    InitPipelines();
    InitImgUi();
    
    // everything went fine
    bIsInitialized = true;
    
    InitDefaultData();
}

void VulkanEngine::cleanup()
{
    if (bIsInitialized) {

        vkDeviceWaitIdle(Device);
        
        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            //already written from before
            vkDestroyCommandPool(Device, Frames[i].CommandPool, nullptr);
        
            //destroy sync objs
            vkDestroyFence(Device, Frames[i].RenderFence, nullptr);
            vkDestroySemaphore(Device, Frames[i].RenderSemaphore, nullptr);
            vkDestroySemaphore(Device, Frames[i].SwapchainSemaphore, nullptr);
            
            Frames[i].DeletionQueue.Flush();
        }
        
        for (auto& Mesh : TestMeshes)
        {
            DestroyBuffer(Mesh->MeshBuffers.IndexBuffer);
            DestroyBuffer(Mesh->MeshBuffers.VertexBuffer);
        }
        
        MainDeletionQueue.Flush();
        
        SDL_DestroyWindow(Window);
        DestroySwapchain();
        
        vkDestroySurfaceKHR(Instance, Surface, nullptr);
        vkDestroyDevice(Device, nullptr);
        
        vkb::destroy_debug_utils_messenger(Instance, DebugMessenger);
        vkDestroyInstance(Instance, nullptr);
        SDL_DestroyWindow(Window);
    }
        

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::DrawBackground(VkCommandBuffer Cmd)
{
    //make a clear-color from frame number. Flash with 120 frame perios
    constexpr float FLASH_PERIOD = 120;
    VkClearColorValue ClearValue;
    float Flash = std::abs(std::sin(FrameNumber / FLASH_PERIOD));
    ClearValue = { { 0, 0, Flash, 1 } };
    
    VkImageSubresourceRange ClearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    
    FComputeEffect& Effect = BackgroundEffects[CurrentBackgroundEffect];
    
    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Effect.Pipeline);
    
    // bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, GradientPipelineLayout, 0, 1, &DrawImageDescriptors, 0, nullptr);
    
    FComputePushConstants Pc{};
    Pc.Data1 = FVector4(1, 0, 0, 1);
    Pc.Data2 = FVector4(0, 0, 1, 1);
    
    vkCmdPushConstants(Cmd, GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FComputePushConstants), &Effect.Data);
    
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(Cmd, std::ceil(DrawExtent.width / 16), std::ceil(DrawExtent.height / 16), 1);
}


void VulkanEngine::DrawGeometry(VkCommandBuffer Cmd)
{
    //begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo ColorAttachment = vkinit::attachment_info(DrawImage.ImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo DepthAttachment = vkinit::depth_attachment_info(DepthImage.ImageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo RenderInfo = vkinit::rendering_info(DrawExtent, &ColorAttachment, &DepthAttachment);
    vkCmdBeginRendering(Cmd, &RenderInfo);


    //set dynamic viewport and scissor
    VkViewport Viewport = {};
    Viewport.x = 0;
    Viewport.y = 0;
    Viewport.width = DrawExtent.width;
    Viewport.height = DrawExtent.height;
    Viewport.minDepth = 0.f;
    Viewport.maxDepth = 1.f;

    vkCmdSetViewport(Cmd, 0, 1, &Viewport);

    VkRect2D Scissor = {};
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = Viewport.width;
    Scissor.extent.height = Viewport.height;
    

    vkCmdSetScissor(Cmd, 0, 1, &Scissor);

//> meshdraw
    
    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, MeshPipeline);

    FGpuDrawPushConstants PushConstants;
    
    FMatrix View = glm::translate( FVector{0.f,0.f,-5.f} );
    
    FMatrix Projection = glm::perspective(glm::radians(70.f), (float)DrawExtent.width / (float)DrawExtent.height, 10000.f, 0.1f);
    
    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    Projection[1][1] *= -1;
    
    
    PushConstants.WorldMatrix = Projection * View;
    //PushConstants.WorldMatrix = {1};
    PushConstants.VertexBuffer = Rectangle.VertexBufferAddress;

    PushConstants.VertexBuffer = TestMeshes[2]->MeshBuffers.VertexBufferAddress;

    vkCmdPushConstants(Cmd, MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(FGpuDrawPushConstants), &PushConstants);
    vkCmdBindIndexBuffer(Cmd, TestMeshes[2]->MeshBuffers.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(Cmd, TestMeshes[2]->Surfaces[0].Count, 1, TestMeshes[2]->Surfaces[0].StartIndex, 0, 0);
//< meshdraw

    vkCmdEndRendering(Cmd);
}

void VulkanEngine::Draw()
{
    constexpr uint64_t TIMEOUT = SToNs(1);
    
    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(Device, 1, &GetCurrentFrame().RenderFence, true, TIMEOUT));
    VK_CHECK(vkResetFences(Device, 1, &GetCurrentFrame().RenderFence));
    
    GetCurrentFrame().DeletionQueue.Flush();
    
    //request image from swapchain
    uint32_t SwapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(Device, Swapchain, TIMEOUT, GetCurrentFrame().SwapchainSemaphore, nullptr, &SwapchainImageIndex));
    
    VkCommandBuffer Cmd = GetCurrentFrame().MainCommandBuffer;
    
    VK_CHECK(vkResetCommandBuffer(Cmd, 0));
    
    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    DrawExtent.width = DrawImage.ImageExtent.width;
    DrawExtent.height = DrawImage.ImageExtent.height;
    
    //Start the command recording
    VK_CHECK(vkBeginCommandBuffer(Cmd, &cmdBeginInfo));
    
    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::TransitionImage(Cmd, DrawImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    DrawBackground(Cmd);
    
    vkutil::TransitionImage(Cmd, DrawImage.Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(Cmd, DepthImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    
    DrawGeometry(Cmd);
    
    VkImage& CurrentSwapImg = SwapchainImages[SwapchainImageIndex];
    
    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::TransitionImage(Cmd, DrawImage.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(Cmd, CurrentSwapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    //Execute a copy from draw to swapchain
    vkutil::CopyImageToImage(Cmd, DrawImage.Image, CurrentSwapImg, DrawExtent, SwapchainExtent);
    
    //Set swapchain image to present
    vkutil::TransitionImage(Cmd, CurrentSwapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    //draw imgui into the swapchain image
    DrawImGui(Cmd, SwapchainImageViews[SwapchainImageIndex]);
    
    // set swapchain image layout to Present so we can draw it
    vkutil::TransitionImage(Cmd, CurrentSwapImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    
    VK_CHECK(vkEndCommandBuffer(Cmd));
    
    //prepare the submission to the queue. 
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo CmdInfo = vkinit::command_buffer_submit_info(Cmd);
    
    VkSemaphoreSubmitInfo WaitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().RenderSemaphore);
    
    VkSubmitInfo2 Submit = vkinit::submit_info(&CmdInfo, &SignalInfo, &WaitInfo);
    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    
    VK_CHECK(vkQueueSubmit2(GraphicsQueue, 1, &Submit, GetCurrentFrame().RenderFence));
    
    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    
    VkPresentInfoKHR PresentInfo {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.pNext = nullptr;
    PresentInfo.pSwapchains = &Swapchain;
    PresentInfo.swapchainCount = 1;
    
    PresentInfo.pWaitSemaphores = &GetCurrentFrame().RenderSemaphore;
    PresentInfo.waitSemaphoreCount = 1;
    
    PresentInfo.pImageIndices = &SwapchainImageIndex;
    
    VK_CHECK(vkQueuePresentKHR(GraphicsQueue, &PresentInfo));
    
    //increase number drawn frames
    ++FrameNumber;
    

}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    bStopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    bStopRendering = false;
                }
            }
            
            //send SDL event to imgui for handling
            ImGui_ImplSDL2_ProcessEvent(&e);
            
            if (e.type == SDL_KEYDOWN)
            {
                fmt::println("Pressed key {:s}", SDL_GetScancodeName(e.key.keysym.scancode));
                
                if (e.key.keysym.sym == SDLK_ESCAPE) bQuit = true;
            }
        }

        // do not draw if we are minimized
        if (bStopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        //imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        //Shome Imgui Uo to test
        //ImGui::ShowDemoWindow();
        if (ImGui::Begin("Background"))
        {
            FComputeEffect& Selected = BackgroundEffects[CurrentBackgroundEffect];
            
            ImGui::Text("Selected Effect: ", Selected.Name);
            
            ImGui::SliderInt("Effect Index", &CurrentBackgroundEffect, 0, BackgroundEffects.size() - 1);
            
            ImGui::SliderFloat4("Data1", (float*)&Selected.Data.Data1, 0.0, 1.0);
            ImGui::SliderFloat4("Data2", (float*)&Selected.Data.Data2, 0.0, 1.0);
            ImGui::SliderFloat4("Data3", (float*)&Selected.Data.Data3, 0.0, 1.0);
            ImGui::SliderFloat4("Data4", (float*)&Selected.Data.Data4, 0.0, 1.0);
        }
        ImGui::End();
        
        //Make imgui calculate internal draw structures
        ImGui::Render();

        Draw();
    }
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer Cmd)>&& Function)
{
    VK_CHECK(vkResetFences(Device, 1, &ImmFence));
    VK_CHECK(vkResetCommandBuffer(ImmCommandBuffer, 0));
    
    VkCommandBuffer Cmd = ImmCommandBuffer;
    
    VkCommandBufferBeginInfo CmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(Cmd, &CmdBeginInfo));
    
    Function(Cmd);
    
    VK_CHECK(vkEndCommandBuffer(Cmd));
    
    VkCommandBufferSubmitInfo CmdInfo = vkinit::command_buffer_submit_info(Cmd);
    VkSubmitInfo2 Submit = vkinit::submit_info(&CmdInfo, nullptr, nullptr);
    
    // submit command buffer to the queue and execute it.
    // RenderFence will now block until the graphic commands finish execution
    
    VK_CHECK(vkQueueSubmit2(GraphicsQueue, 1, &Submit, ImmFence));
    
    VK_CHECK(vkWaitForFences(Device, 1, &ImmFence, true, UINT64_MAX));

}

void VulkanEngine::DrawImGui(VkCommandBuffer Cmd, VkImageView TargetImageView)
{
    VkRenderingAttachmentInfo ColorAttachment = vkinit::attachment_info(TargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo RenderInfo = vkinit::rendering_info(SwapchainExtent, &ColorAttachment, nullptr);
    
    vkCmdBeginRendering(Cmd, &RenderInfo);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Cmd);
    vkCmdEndRendering(Cmd);
}

FAllocatedBuffer VulkanEngine::CreateBuffer(size_t AllocSize, VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage)
{
    //Allocate buffer
    VkBufferCreateInfo BufferInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    BufferInfo.pNext = nullptr;
    BufferInfo.size = AllocSize;
    
    BufferInfo.usage = Usage;
    
    VmaAllocationCreateInfo AllocInfo{};
    AllocInfo.usage = MemoryUsage;
    AllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    FAllocatedBuffer NewBuffer;
    
    //Allocate the buffer
    VK_CHECK(vmaCreateBuffer(Allocator, &BufferInfo, &AllocInfo, &NewBuffer.Buffer, &NewBuffer.Allocation, &NewBuffer.Info));
    
    return NewBuffer;
}

void VulkanEngine::DestroyBuffer(const FAllocatedBuffer& Buffer)
{
    vmaDestroyBuffer(Allocator, Buffer.Buffer, Buffer.Allocation);
}

FGpuMeshBuffers VulkanEngine::UploadMesh(std::span<uint32> Indices, std::span<FVertex> Vertices)
{
    const size_t VertexBufferSize = Vertices.size() * sizeof(FVertex);
    const size_t IndexBufferSize = Indices.size() * sizeof(uint32);
    
    FGpuMeshBuffers NewSurface;
    
    //Create vertex buffer
    NewSurface.VertexBuffer = CreateBuffer(VertexBufferSize, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    
    //find the address of the vertex buffer
    VkBufferDeviceAddressInfo DeviceAddressInfo {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = NewSurface.VertexBuffer.Buffer };
    NewSurface.VertexBufferAddress = vkGetBufferDeviceAddress(Device, &DeviceAddressInfo);
    
    //create index buffer
    NewSurface.IndexBuffer = CreateBuffer(IndexBufferSize, 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    
    FAllocatedBuffer Staging = CreateBuffer(VertexBufferSize + IndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* Data = Staging.Allocation->GetMappedData();
    
    //Copy vertex buffer
    FMemory::Memcpy(Data, Vertices.data(), VertexBufferSize);
    
    //Copy index buffer
    FMemory::Memcpy(static_cast<char*>(Data) + VertexBufferSize, Indices.data(), IndexBufferSize);
    
    ImmediateSubmit([&](VkCommandBuffer Cmd)
    {
        VkBufferCopy VertexCopy{0};
        VertexCopy.dstOffset = 0;
        VertexCopy.srcOffset = 0;
        VertexCopy.size = VertexBufferSize;
        
        vkCmdCopyBuffer(Cmd, Staging.Buffer, NewSurface.VertexBuffer.Buffer, 1, &VertexCopy);
        
        VkBufferCopy IndexCopy{0};
        IndexCopy.dstOffset = 0;
        IndexCopy.srcOffset = VertexBufferSize;
        IndexCopy.size = IndexBufferSize;
        
        vkCmdCopyBuffer(Cmd, Staging.Buffer, NewSurface.IndexBuffer.Buffer, 1, &IndexCopy);
    });
    
    DestroyBuffer(Staging);
    
    return NewSurface;
}

void VulkanEngine::InitImgUi()
{
    // 1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo
    // itself.
    constexpr int K = 1000; //1K
    VkDescriptorPoolSize PoolSizes[] {
        { VK_DESCRIPTOR_TYPE_SAMPLER, K},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, K},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, K},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, K},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, K},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, K},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, K},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, K},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, K},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, K},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, K}
    };
    
    VkDescriptorPoolCreateInfo PoolInfo{};
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    PoolInfo.maxSets = K;
    PoolInfo.poolSizeCount = (uint32_t)std::size(PoolSizes);
    PoolInfo.pPoolSizes = PoolSizes;
    
    VkDescriptorPool ImguiPool;
    VK_CHECK(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &ImguiPool));
    
    //2 Initialize ImgUiLibrary
    
    // this initializes the core structures of imgui
    ImGui::CreateContext();
    
    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(Window);
    
    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo InitInfo{};
    InitInfo.Instance = Instance;
    InitInfo.PhysicalDevice = ChosenGPU;
    InitInfo.Device = Device;
    InitInfo.Queue = GraphicsQueue;
    InitInfo.DescriptorPool = ImguiPool;
    InitInfo.MinImageCount = 3;
    InitInfo.ImageCount = 3;
    InitInfo.UseDynamicRendering = true;
    
    
    //dynamic rendering parameters for imgui to use
    InitInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    InitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    InitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &SwapchainImageFormat;
    
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    ImGui_ImplVulkan_Init(&InitInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
    
    // add the destroy the imgui created structures
    MainDeletionQueue.PushFunction([=]()
    {
       ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(Device, ImguiPool, nullptr);
    });
    
}

void VulkanEngine::InitPipelines()
{
    //COMPUTE PIPELINES
    InitBackgroundPipelines();
    
    // GRAPHICS PIPELINES
    InitTrianglePipeline();
    InitMeshPipeline();
}

void VulkanEngine::InitBackgroundPipelines()
{
    VkPipelineLayoutCreateInfo ComputeLayout{};
    ComputeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ComputeLayout.pNext = nullptr;
    ComputeLayout.pSetLayouts = &DrawImageDescLayout;
    ComputeLayout.setLayoutCount = 1;
    
    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(FComputePushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;
    
    VK_CHECK(vkCreatePipelineLayout(Device, &ComputeLayout, nullptr, &GradientPipelineLayout));
    
    //layout code
    VkShaderModule GradientShader;
    if (!vkutil::LoadShaderModule("gradient_color.comp.spv", Device, GradientShader))
    {
        fmt::println("Error when building the compute shader");
        return;
    }
    
    VkShaderModule SkyShader;
    if (!vkutil::LoadShaderModule("sky.comp.spv", Device, SkyShader))
    {
        fmt::println("Error when building the compute shader");
        return;
    }
    
    VkPipelineShaderStageCreateInfo StageInfo{};
    StageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    StageInfo.pNext = nullptr;
    StageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    StageInfo.module = GradientShader;
    StageInfo.pName = "main";
    
    VkComputePipelineCreateInfo ComputePipelineCreateInfo{};
    ComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ComputePipelineCreateInfo.pNext = nullptr;
    ComputePipelineCreateInfo.layout = GradientPipelineLayout;
    ComputePipelineCreateInfo.stage = StageInfo;
    
    FComputeEffect Gradient;
    Gradient.Layout = GradientPipelineLayout;
    Gradient.Name = "Gradient";
    Gradient.Data = {};
    
    //default colors
    Gradient.Data.Data1 = FVector4(1, 0, 0, 1);
    Gradient.Data.Data2 = FVector4(0, 0, 1, 1);
    
    VK_CHECK(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &ComputePipelineCreateInfo, nullptr, &Gradient.Pipeline));
    
    //change the module only to create the sky shader
    ComputePipelineCreateInfo.stage.module = SkyShader;
    
    FComputeEffect Sky;
    Sky.Layout = GradientPipelineLayout;
    Sky.Name = "Sky";
    Sky.Data = {};
    
    //default sky parameters
    Sky.Data.Data1 = FVector4(1, 1, 1, 0.97);
    Sky.Data.Data2 = FVector4(.2, -.06, 0, 0);
    Sky.Data.Data3 = FVector4( 0.1, 0.2, 0.4 , 0);
    
    VK_CHECK(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &ComputePipelineCreateInfo, nullptr, &Sky.Pipeline));
    
    //Add the 2 background effects to the array
    BackgroundEffects.push_back(Gradient);
    BackgroundEffects.push_back(Sky);
    
    vkDestroyShaderModule(Device, GradientShader, nullptr);
    vkDestroyShaderModule(Device, SkyShader, nullptr);
    
    MainDeletionQueue.PushFunction([=, this]()
    {
        vkDestroyPipelineLayout(Device, GradientPipelineLayout, nullptr);
        vkDestroyPipeline(Device, Sky.Pipeline, nullptr);
        vkDestroyPipeline(Device, Gradient.Pipeline, nullptr);
    });
}

void VulkanEngine::InitTrianglePipeline()
{
    VkShaderModule TriangleFragShader;
    if (!vkutil::LoadShaderModule("ColoredTriangle.frag.spv", Device, TriangleFragShader))
    {
        fmt::println("Error when building the fragment shader");
    }
    else
    {
        fmt::println("Triangle fragment shader succesfully loaded");
    }
    
    VkShaderModule TriangleVertexShader;
    if (!vkutil::LoadShaderModule("ColoredTriangle.vert.spv", Device, TriangleVertexShader))
    {
        fmt::println("Error when building the vertex shader");
    }
    else
    {
        fmt::println("Triangle vertex shader succesfully loaded");
    }
    
    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(Device, &PipelineLayoutInfo, nullptr, &TrianglePipelineLayout));
    
    FPipelineBuilder PipelineBuilder;
    
    //Use created triangle layout
    PipelineBuilder.Layout = TrianglePipelineLayout;
    //Connect the vertex and pixel shaders to pipeline
    PipelineBuilder.SetShaders(TriangleVertexShader, TriangleFragShader);
    //it will draw triangles
    PipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    PipelineBuilder.SetPoligonMode(VK_POLYGON_MODE_FILL);
    //no backface culling
    PipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no mulisampling
    PipelineBuilder.SetMultisamplingNone();
    //no blending
    PipelineBuilder.DisableBlending();
    
    // no depth testing
    //PipelineBuilder.DisableDepthTest();
    PipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    
    //Connect the image format we wl draw into, from draw image
    PipelineBuilder.SetColorAttachmentFormat(DrawImage.ImageFormat);
    PipelineBuilder.SetDepthFormat(DepthImage.ImageFormat);
    
    //Finally build the pipeline
    TrianglePipeline = PipelineBuilder.BuildPipeline(Device);
    
    //Clean structures
    vkDestroyShaderModule(Device, TriangleFragShader, nullptr);
    vkDestroyShaderModule(Device, TriangleVertexShader, nullptr);
    
    MainDeletionQueue.PushFunction([=, this]()
    {
       vkDestroyPipelineLayout(Device, TrianglePipelineLayout, nullptr); 
       vkDestroyPipeline(Device, TrianglePipeline, nullptr); 
    });
}

void VulkanEngine::InitMeshPipeline()
{
    VkShaderModule TriangleFragShader;
    if (!vkutil::LoadShaderModule("ColoredTriangle.frag.spv", Device, TriangleFragShader))
    {
        fmt::println("Error when building the fragment shader");
    }
    else
    {
        fmt::println("Triangle fragment shader succesfully loaded");
    }
    
    VkShaderModule TriangleVertexShader;
    if (!vkutil::LoadShaderModule("ColoredTriangleMesh.vert.spv", Device, TriangleVertexShader))
    {
        fmt::println("Error when building the shader shader");
    }
    else
    {
        fmt::println("Triangle vertex shader succesfully loaded");
    }
    
    VkPushConstantRange BufferRange{};
    BufferRange.offset = 0;
    BufferRange.size = sizeof(FGpuDrawPushConstants);
    BufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    PipelineLayoutInfo.pPushConstantRanges = &BufferRange;
    PipelineLayoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(Device, &PipelineLayoutInfo, nullptr, &MeshPipelineLayout));
    
    FPipelineBuilder PipelineBuilder;
    
    //Use created triangle layout
    PipelineBuilder.Layout = MeshPipelineLayout;
    //Connect the vertex and pixel shaders to pipeline
    PipelineBuilder.SetShaders(TriangleVertexShader, TriangleFragShader);
    //it will draw triangles
    PipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    PipelineBuilder.SetPoligonMode(VK_POLYGON_MODE_FILL);
    //no backface culling
    PipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no mulisampling
    PipelineBuilder.SetMultisamplingNone();
    //no blending
    PipelineBuilder.DisableBlending();
    // no depth testing
    PipelineBuilder.DisableDepthTest();
    
    //Connect the image format we wl draw into, from draw image
    PipelineBuilder.SetColorAttachmentFormat(DrawImage.ImageFormat);
    PipelineBuilder.SetDepthFormat(DepthImage.ImageFormat);
    
    //Finally build the pipeline
    MeshPipeline = PipelineBuilder.BuildPipeline(Device);
    
    //Clean structures
    vkDestroyShaderModule(Device, TriangleFragShader, nullptr);
    vkDestroyShaderModule(Device, TriangleVertexShader, nullptr);
    
    
    MainDeletionQueue.PushFunction([=, this]()
    {
       vkDestroyPipelineLayout(Device, MeshPipelineLayout, nullptr); 
       vkDestroyPipeline(Device, MeshPipeline, nullptr); 
    });
}

void VulkanEngine::InitDefaultData()
{
    TStaticArray<FVertex, 4> RectVertices{};
    
    
    RectVertices[0].Position = {0.5,-0.5, 0};
    RectVertices[1].Position = {0.5,0.5, 0};
    RectVertices[2].Position = {-0.5,-0.5, 0};
    RectVertices[3].Position = {-0.5,0.5, 0};

    RectVertices[0].Color = {0,0, 0,1};
    RectVertices[1].Color = { 0.5,0.5,0.5 ,1};
    RectVertices[2].Color = { 1,0, 0,1 };
    RectVertices[3].Color = { 0,1, 0,1 };

    TStaticArray<uint32,6> RectIndices;

    RectIndices[0] = 0;
    RectIndices[1] = 1;
    RectIndices[2] = 2;

    RectIndices[3] = 2;
    RectIndices[4] = 1;
    RectIndices[5] = 3;
    
    Rectangle = UploadMesh(RectIndices, RectVertices);
    
    //Delete rectangle data on engine shutdown
    MainDeletionQueue.PushFunction([=, this]()
    {
        DestroyBuffer(Rectangle.IndexBuffer);
        DestroyBuffer(Rectangle.VertexBuffer);
    });
    
    TestMeshes = vkLoader::LoadGltfMeshes(*this, "Meshes/basicmesh.glb").value();
    
    
}

void VulkanEngine::InitDescriptors()
{
    //create a descriptor pool that will hold 10 sets with 1 image each
    TArray<FDescriptorAllocator::FPoolSizeRation> Sizes {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };
    
    GlobalDescriptorAllocator.InitPool(Device, 10, Sizes);
    
    //make the descriptor set layout for our compute draw
    {
        FDescriptorLayoutBuilder Builder;
        Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        DrawImageDescLayout = Builder.Build(Device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
    }
    
    //allocate a descriptor set for our draw image
    DrawImageDescriptors = GlobalDescriptorAllocator.Allocate(Device, DrawImageDescLayout);
    
    VkDescriptorImageInfo ImgInfo{};
    ImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgInfo.imageView = DrawImage.ImageView;
    
    VkWriteDescriptorSet DrawImgWrite{};
    DrawImgWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DrawImgWrite.pNext = nullptr;
    
    DrawImgWrite.dstBinding = 0;
    DrawImgWrite.dstSet = DrawImageDescriptors;
    DrawImgWrite.descriptorCount = 1;
    DrawImgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    DrawImgWrite.pImageInfo = &ImgInfo;
    
    vkUpdateDescriptorSets(Device, 1, &DrawImgWrite, 0, nullptr);
    
    //make sure both the descriptor allocator and the new layout get cleaned up properly
	MainDeletionQueue.PushFunction([=, this]()
	{
	    GlobalDescriptorAllocator.DestroyPool(Device);
	    
	    vkDestroyDescriptorSetLayout(Device, DrawImageDescLayout, nullptr);
	});
    
}

void VulkanEngine::InitVulkan()
{
    vkb::InstanceBuilder builder;
    
    //make instance with debug
    auto inst_ret = builder.set_app_name("Chimera Engine")
    .request_validation_layers(bUseValidationLayers)
    .use_default_debug_messenger()
    .require_api_version(1, 3, 0)
    .build();
    
    vkb::Instance vkb_inst = inst_ret.value();
    
    Instance = vkb_inst.instance;
    DebugMessenger = vkb_inst.debug_messenger;
    
    SDL_Vulkan_CreateSurface(Window, Instance, &Surface);
    
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;
    
    VkPhysicalDeviceVulkan12Features features12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    
    //Select GPU
    vkb::PhysicalDeviceSelector Selector { vkb_inst };
    vkb::PhysicalDevice PhysicalDevice = Selector
        .set_minimum_version(1,3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(Surface)
        .select()
        .value();
    
    vkb::DeviceBuilder deviceBuilder { PhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();
    
    Device = vkbDevice.device;
    ChosenGPU = PhysicalDevice.physical_device;
    
    // --------- Initialize Graphics Queue ----------------
    GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
 
    
    //Initialize memory allocator
    VmaAllocatorCreateInfo AllocatorInfo{};
    AllocatorInfo.physicalDevice = ChosenGPU;
    AllocatorInfo.device = Device;
    AllocatorInfo.instance = Instance;
    AllocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&AllocatorInfo, &Allocator);
    
    MainDeletionQueue.PushFunction([=, this]()
    {
        vmaDestroyAllocator(Allocator);
    });
    
}

void VulkanEngine::InitSwapchain()
{
    CreateSwapchain(WindowExtent.width, WindowExtent.height);
    
    //Draw image size match window
    VkExtent3D DrawImageExtent{
        WindowExtent.width,
        WindowExtent.height,
        1
    };
    
    //Hardcode draw format to 32bit float
    DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    DrawImage.ImageExtent = DrawImageExtent;
    
    VkImageUsageFlags DrawImageUsages{};
    DrawImageUsages |= (
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        |   VK_IMAGE_USAGE_TRANSFER_DST_BIT
        |   VK_IMAGE_USAGE_STORAGE_BIT
        |   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );
    
    VkImageCreateInfo RImgInfo = vkinit::image_create_info(DrawImage.ImageFormat, DrawImageUsages, DrawImageExtent);
    
    //for draw image, allocate it from gpu local memory
    VmaAllocationCreateInfo RImgAllocInfo{};
    RImgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    RImgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    //allocation and creation
    vmaCreateImage(Allocator, &RImgInfo, &RImgAllocInfo, &DrawImage.Image, &DrawImage.Allocation, nullptr);
    
    //build image-view for draw image to use for rendering
    VkImageViewCreateInfo RViewInfo = vkinit::imageview_create_info(DrawImage.ImageFormat, DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
    
    VK_CHECK(vkCreateImageView(Device, &RViewInfo, nullptr, &DrawImage.ImageView));
    
    DepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
    DepthImage.ImageExtent = DrawImageExtent;
    
    VkImageUsageFlags DepthImageUsages{};
    DepthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    VkImageCreateInfo DepthImgInfo = vkinit::image_create_info(DepthImage.ImageFormat, DepthImageUsages, DepthImage.ImageExtent);
    
    //allocate and create the image
    vmaCreateImage(Allocator, &DepthImgInfo, &RImgAllocInfo, &DepthImage.Image, &DepthImage.Allocation, nullptr);
    
    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo DepthViewInfo = vkinit::imageview_create_info(DepthImage.ImageFormat, DepthImage.Image, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    VK_CHECK(vkCreateImageView(Device, &DepthViewInfo, nullptr, &DepthImage.ImageView));
    
    //Add tp deletion queue
    MainDeletionQueue.PushFunction([this]()
    {
        vkDestroyImageView(Device, DrawImage.ImageView, nullptr);
        vmaDestroyImage(Allocator, DrawImage.Image, DrawImage.Allocation);
        
        vkDestroyImageView(Device, DepthImage.ImageView, nullptr);
        vmaDestroyImage(Allocator, DepthImage.Image, DepthImage.Allocation);
    });
}

void VulkanEngine::InitCommands()
{
    //create a command pool for commands submitted to the graphics queue.
    //we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo CommandPoolInfo {};
    CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.pNext = nullptr;
    CommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CommandPoolInfo.queueFamilyIndex = GraphicsQueueFamily;

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        VK_CHECK(vkCreateCommandPool(Device, &CommandPoolInfo, nullptr, &Frames[i].CommandPool));
        
        //allocate default command buffer for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = nullptr;
        cmdAllocInfo.commandPool = Frames[i].CommandPool;
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        
        VK_CHECK(vkAllocateCommandBuffers(Device, &cmdAllocInfo, &Frames[i].MainCommandBuffer));
    }
    
    VK_CHECK(vkCreateCommandPool(Device, &CommandPoolInfo, nullptr, &ImmCommandPool));
    
    //Allocate command buffer for immediate submits
    VkCommandBufferAllocateInfo CmdAllocInfo = vkinit::command_buffer_allocate_info(ImmCommandPool, 1);
    
    VK_CHECK(vkAllocateCommandBuffers(Device, &CmdAllocInfo, &ImmCommandBuffer));
    
    MainDeletionQueue.PushFunction([this]()
    {
        vkDestroyCommandPool(Device, ImmCommandPool, nullptr);
    });
}

void VulkanEngine::InitSyncStructures()
{
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo FenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        VK_CHECK(vkCreateFence(Device, &FenceCreateInfo, nullptr, &Frames[i].RenderFence));
        
        VK_CHECK(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Frames[i].SwapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Frames[i].RenderSemaphore));
    }
    
    VK_CHECK(vkCreateFence(Device, &FenceCreateInfo, nullptr, &ImmFence));
    MainDeletionQueue.PushFunction([this]()
    {
        vkDestroyFence(Device, ImmFence, nullptr);
    });
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder { ChosenGPU, Device, Surface };
    
    SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    vkb::Swapchain vkbSwapchain = swapchainBuilder
    //.use_default_format_selection()
    .set_desired_format(VkSurfaceFormatKHR{ .format = SwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
    //VSync
    .set_desired_present_mode(bVSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
    .set_desired_extent(width, height)
    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    .build()
    .value();
    
    SwapchainExtent = vkbSwapchain.extent;
    
    //store swapchain and related images
    Swapchain = vkbSwapchain.swapchain;
    SwapchainImages = vkbSwapchain.get_images().value();
    SwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
    
    //destroy resources
    for (int i = 0; i < SwapchainImageViews.size(); ++i)
    {
        vkDestroyImageView(Device, SwapchainImageViews[i], nullptr);
    }
}

