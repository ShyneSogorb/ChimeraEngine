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
#include "Math.h"
#include "Memory.h"
#include "Transform.h"

#define DRAW_TRIANGLE 0

VulkanEngine* LoadedEngine = nullptr;

constexpr bool bUseValidationLayers = true; 
constexpr bool bVSync = true;

namespace FTimeHelper
{
    enum class ETime : uint8
    {
        Seconds = 0,
        Milliseconds = 1,
        Microseconds = 2,
        Nanoseconds = 3,
    };
    
    constexpr int32 Abs(int32 N) { return N < 0 ? -N : N; }
    
    constexpr uint64 ConvertTimeTo(uint64 Time, ETime From, ETime To)
    {
        if (From == To) return Time;
        
        const int32 Diff = static_cast<int32>(To) - static_cast<int32>(From);
        
        uint64 Factor = 1;
        
        for (uint32 i = 0; i < Abs(Diff); ++i)
            Factor *= 1000;
        
        return Diff > 0 ? Time * Factor : Time / Factor;
    }
}


VulkanEngine& VulkanEngine::Get() { return *LoadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(LoadedEngine == nullptr);
    LoadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags WindowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    Window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WindowExtent.width,
        WindowExtent.height,
        WindowFlags
    );

    
    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();
    InitDescriptors();
    
    InitPipelines();
    
    InitDefaultData();
        
    InitRenderable();
    
    InitImgUi();
    
    // everything went fine
    bIsInitialized = true;
    
    
    MainCamera.Reset();

    
     
    
}

void VulkanEngine::cleanup()
{
    if (bIsInitialized) {

        vkDeviceWaitIdle(Device);
        
        LoadedScenes.clear();
        
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
        
        MainDeletionQueue.Flush();
        
        SDL_DestroyWindow(Window);
        DestroySwapchain();
        
        vkDestroySurfaceKHR(Instance, WindowSurface, nullptr);
        vkDestroyDevice(Device, nullptr);
        
        vkb::destroy_debug_utils_messenger(Instance, DebugMessenger);
        vkDestroyInstance(Instance, nullptr);
        SDL_DestroyWindow(Window);
    }
        

    // clear engine pointer
    LoadedEngine = nullptr;
}

void VulkanEngine::DrawBackground(VkCommandBuffer Cmd)
{
    //make a clear-color from frame number. Flash with 120 frame perios
    constexpr float FLASH_PERIOD = 120;
    VkClearColorValue ClearValue;
    float Flash = FMath::Abs(FMath::Sin(FrameNumber / FLASH_PERIOD));
    ClearValue = { { 0, 0, Flash, 1 } };
    
    VkImageSubresourceRange ClearRange = Vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    
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
    vkCmdDispatch(Cmd, FMath::Ceil(DrawExtent.width / 16), FMath::Ceil(DrawExtent.height / 16), 1);
}


void VulkanEngine::DrawGeometry(VkCommandBuffer Cmd)
{
    
    //reset counters
    Stats.DrawcallCount = 0;
    Stats.TriangleCount = 0;
    //begin clock
    auto Start = std::chrono::system_clock::now();
    
    //begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo ColorAttachment = Vkinit::attachment_info(DrawImage.ImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo DepthAttachment = Vkinit::depth_attachment_info(DepthImage.ImageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo RenderInfo = Vkinit::rendering_info(DrawExtent, &ColorAttachment, &DepthAttachment);
    vkCmdBeginRendering(Cmd, &RenderInfo);

    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, TrianglePipeline);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = DrawExtent.width;
    viewport.height = DrawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(Cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;

    vkCmdSetScissor(Cmd, 0, 1, &scissor);
    
    //Allocate a new uniform buffer for the scene data
    FAllocatedBuffer GpuSceneDataBuffer = CreateBuffer(sizeof(FGpuSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
    //Add it to the deletion queue of this frame so it gets deleted once its been used
    GetCurrentFrame().DeletionQueue.PushFunction([=, this]()
    {
       DestroyBuffer(GpuSceneDataBuffer); 
    });
    
    //Write the buffer
    FGpuSceneData* SceneUniformData = (FGpuSceneData*)GpuSceneDataBuffer.Allocation->GetMappedData();
    *SceneUniformData = SceneData;
    
    //Create a descriptor set tha binds that buffer and update it
    VkDescriptorSet GlobalDescriptor = GetCurrentFrame().FrameDescriptors.Allocate(Device, GpuSceneDataDescriptorLayout);
    
    FDescriptorWriter Writer;
    Writer.WriteBuffer(0, GpuSceneDataBuffer.Buffer, sizeof(FGpuSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Writer.UpdateSet(Device, GlobalDescriptor);

    auto Draw = [&](const FRenderObject& Draw)
    {
        vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Draw.MaterialInstance->Pipeline->Pipeline);
        vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Draw.MaterialInstance->Pipeline->Layout, 0, 1, &GlobalDescriptor, 0, nullptr);
        vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Draw.MaterialInstance->Pipeline->Layout, 1, 1, &Draw.MaterialInstance->MaterialSet, 0, nullptr);
        
        vkCmdBindIndexBuffer(Cmd, Draw.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
        
        FGpuDrawPushConstants Push{};
        Push.VertexBuffer = Draw.VertexBufferAddress;
        Push.WorldMatrix = Draw.Transform;
        vkCmdPushConstants(Cmd, Draw.MaterialInstance->Pipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &Push);
        
        vkCmdDrawIndexed(Cmd, Draw.IndexCount, 1, Draw.FirstIndex, 0, 0);
        
        //add counters for triangles and draws
        Stats.DrawcallCount++;
        Stats.TriangleCount += Draw.IndexCount / 3;   
    };
    
    for (const FRenderObject& RenderObject : MainDrawContext.OpaqueSurfaces)
    {
        Draw(RenderObject);
    }
    
    for (const FRenderObject& RenderObject : MainDrawContext.TransparentSurfaces)
    {
        Draw(RenderObject);
    }

    vkCmdEndRendering(Cmd);
    
    MainDrawContext.OpaqueSurfaces.clear();
    MainDrawContext.TransparentSurfaces.clear();
    
    auto End = std::chrono::system_clock::now();
        
    //convert to microseconds (integer), then back to miliseconds
    auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start);
    Stats.MeshDrawTime = Elapsed.count() / 1000.f;
    
}

void VulkanEngine::UpdateScene()
{
    auto Start = std::chrono::system_clock::now();
    
    MainCamera.Update();
    
    FMatrix View = MainCamera.GetViewMatrix();
    
    //Camera projection
    FMatrix Projection = FTransform::Perspective(FTransform::Radians(70.f), (float)WindowExtent.width / WindowExtent.height, 10000.f, .1f);
    
    //LoadedNodes["Suzanne"]->Draw(FMatrix{1.f}, MainDrawContext);
    
    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    Projection[1][1] *= -1;
    
    SceneData.View = View;
    SceneData.Projection = Projection;
    SceneData.ViewProjection = Projection * View;
    
    //Some default lighting parameters
    SceneData.AmbientColor = FColor{.1, .1f, .1f, 1.f};
    SceneData.SunlightColor = FLinearColor::White;
    SceneData.SunlightDirection = FColor{0, 1, .5, 1.};
    
    for (int32 i = -3; i < 3; ++i)
    {
        FMatrix Scale = FTransform::Scale(FVector{.2});
        FMatrix Translation = FTransform::Translate(FVector3{i, 1, 0});
        
        //LoadedNodes["Cube"]->Draw(Translation * Scale, MainDrawContext);
    }
    
    LoadedScenes["Structure"]->Draw(FMatrix{1.f}, MainDrawContext);
    
        
    auto End = std::chrono::system_clock::now();
        
    //convert to microseconds (integer), then back to miliseconds
    auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start);
    Stats.SceneUpdateTime = Elapsed.count() / 1000.f;
    
}

void FMeshNode::Draw(const FMatrix& TopMatrix, FDrawContext& Ctx)
{
    FMatrix NodeMatrix = TopMatrix * WorldTransform;
    
    for (auto& Surface : Mesh->Surfaces)
    {
        FRenderObject Def;
        Def.IndexCount = Surface.Count;
        Def.FirstIndex = Surface.StartIndex;
        Def.IndexBuffer = Mesh->MeshBuffers.IndexBuffer.Buffer;
        Def.MaterialInstance = &Surface.Material->Data;
        
        Def.Transform = NodeMatrix;
        Def.VertexBufferAddress = Mesh->MeshBuffers.VertexBufferAddress;
        
        Ctx.OpaqueSurfaces.push_back(Def);
    }
    
    FNode::Draw(TopMatrix, Ctx);
}

void FGltfMetalicRoughness::BuildPipelines(VulkanEngine& Engine)
{
    VkShaderModule MeshFragShader;
    if (vkutil::LoadShaderModule("Mesh.frag.spv", Engine.Device, MeshFragShader)) {
        fmt::println("Note Success when building the triangle fragment shader module");
    }
    else {
        fmt::println("Error when building the triangle fragment shader module");
    }
    
    VkShaderModule MeshVertexShader;
    if (vkutil::LoadShaderModule("Mesh.vert.spv", Engine.Device, MeshVertexShader)) {
        fmt::println("Note Success when building the triangle vertex shader module");
    }
    else {
        fmt::println("Error when building the triangle vertex shader module");
    }
    
    VkPushConstantRange MatrixRange{};
    MatrixRange.offset = 0;
    MatrixRange.size = sizeof(FGpuDrawPushConstants);
    MatrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    FDescriptorLayoutBuilder LayoutBuilder;
    LayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    LayoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    LayoutBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    
    MaterialLayout = LayoutBuilder.Build(Engine.Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
    VkDescriptorSetLayout Layouts[]
    {
        Engine.GpuSceneDataDescriptorLayout,
        MaterialLayout
    };
    
    VkPipelineLayoutCreateInfo MeshLayoutInfo = Vkinit::PipelineLayoutCreateInfo();
    MeshLayoutInfo.setLayoutCount = std::size(Layouts);
    MeshLayoutInfo.pSetLayouts = Layouts;
    MeshLayoutInfo.pPushConstantRanges = &MatrixRange;
    MeshLayoutInfo.pushConstantRangeCount = 1;
    
    VkPipelineLayout NewLayout;
    VK_CHECK(vkCreatePipelineLayout(Engine.Device, &MeshLayoutInfo, nullptr, &NewLayout));
    
    OpaquePipeline.Layout = NewLayout;
    TransparentPipeline.Layout = NewLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    FPipelineBuilder PipelineBuilder;
    PipelineBuilder.SetShaders(MeshVertexShader, MeshFragShader);
    PipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    PipelineBuilder.SetPoligonMode(VK_POLYGON_MODE_FILL);
    PipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    PipelineBuilder.SetMultisamplingNone();
    PipelineBuilder.DisableBlending();
    PipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    
    //Render format
    PipelineBuilder.SetColorAttachmentFormat(Engine.DrawImage.ImageFormat);
    PipelineBuilder.SetDepthFormat(Engine.DepthImage.ImageFormat);
    
    //use the created triangle layout
    PipelineBuilder.Layout = NewLayout;
    
    //Finally build the pipeline
    OpaquePipeline.Pipeline = PipelineBuilder.BuildPipeline(Engine.Device);
    
     //Create the transparent variants
    PipelineBuilder.EnableBlendingAdditive();
    PipelineBuilder.EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    
    TransparentPipeline.Pipeline = PipelineBuilder.BuildPipeline(Engine.Device);
    
    vkDestroyShaderModule(Engine.Device, MeshFragShader, nullptr);
    vkDestroyShaderModule(Engine.Device, MeshVertexShader, nullptr);
    
}

FMaterialInstance FGltfMetalicRoughness::WriteMaterial(VkDevice Device, EMaterialPass Pass,
    const FMaterialResources& Resources, FDescriptorAllocator& DescriptorAllocator)
{
    FMaterialInstance Instance;
    Instance.PassType = Pass;
    
    if (Pass == EMaterialPass::Transparent)
    {
        Instance.Pipeline = &TransparentPipeline;
    }
    else
    {
        Instance.Pipeline = &OpaquePipeline;
    }
    
    Instance.MaterialSet = DescriptorAllocator.Allocate(Device, MaterialLayout);
    
    Writer.Clear();
    Writer.WriteBuffer(0, Resources.DataBuffer, sizeof(FMaterialConstants), Resources.DataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Writer.WriteImage(1, Resources.ColorImage.ImageView, Resources.ColorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Writer.WriteImage(2, Resources.MetalRoughImage.ImageView, Resources.MetalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    
    Writer.UpdateSet(Device, Instance.MaterialSet);
    return Instance;
}

void VulkanEngine::Draw()
{
    constexpr uint64_t TIMEOUT = FTimeHelper::ConvertTimeTo(1, FTimeHelper::ETime::Seconds, FTimeHelper::ETime::Nanoseconds);
    
    UpdateScene();
    
    // wait until the gpu has finished rendering the last frame. Timeout
    VK_CHECK(vkWaitForFences(Device, 1, &GetCurrentFrame().RenderFence, true, TIMEOUT));
    VK_CHECK(vkResetFences(Device, 1, &GetCurrentFrame().RenderFence));
    
    
    DrawExtent.height = FMath::Min(SwapchainExtent.height, DrawImage.ImageExtent.height) * RenderScale;
    DrawExtent.width = FMath::Min(SwapchainExtent.width, DrawImage.ImageExtent.width) * RenderScale;
    
    GetCurrentFrame().DeletionQueue.Flush();
    GetCurrentFrame().FrameDescriptors.ClearPools(Device);
    
    //request image from swapchain
    uint32_t SwapchainImageIndex;
    VkResult Error = vkAcquireNextImageKHR(Device, Swapchain, TIMEOUT, GetCurrentFrame().SwapchainSemaphore, nullptr, &SwapchainImageIndex);
    if (Error == VK_ERROR_OUT_OF_DATE_KHR)
    {
        bResizeRequest = true;
        return;
    }
    
    VkCommandBuffer Cmd = GetCurrentFrame().MainCommandBuffer;
    
    VK_CHECK(vkResetCommandBuffer(Cmd, 0));
    
    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = Vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
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

    VkCommandBufferSubmitInfo CmdInfo = Vkinit::command_buffer_submit_info(Cmd);
    
    VkSemaphoreSubmitInfo WaitInfo = Vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalInfo = Vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().RenderSemaphore);
    
    VkSubmitInfo2 Submit = Vkinit::submit_info(&CmdInfo, &SignalInfo, &WaitInfo);
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
    
    Error = vkQueuePresentKHR(GraphicsQueue, &PresentInfo);
    if (Error == VK_ERROR_OUT_OF_DATE_KHR)
    {
        bResizeRequest = true;
        return;
    }
    
    //increase number drawn frames
    ++FrameNumber;
    

}

void VulkanEngine::Run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        
        //begin clock
        auto Start = std::chrono::system_clock::now();
        
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
            
            MainCamera.ProcessSdlEvents(e);
            //send SDL event to imgui for handling
            ImGui_ImplSDL2_ProcessEvent(&e);
            
            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) bQuit = true;
            }
        }

        // do not draw if we are minimized
        if (bStopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        
        if (bResizeRequest) {
            ResizeSwapChain();
        }
        
        //imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        //Shome Imgui Uo to test
        //ImGui::ShowDemoWindow();
        if (ImGui::Begin("Background"))
        {
            
            ImGui::SliderFloat("Render Scale", &RenderScale, .01f, 1.f);
            
            FComputeEffect& Selected = BackgroundEffects[CurrentBackgroundEffect];
            
            ImGui::Text("Selected Effect: ", Selected.Name);
            
            ImGui::SliderInt("Effect Index", &CurrentBackgroundEffect, 0, BackgroundEffects.size() - 1);
            
            ImGui::SliderFloat4("Data1", (float*)&Selected.Data.Data1, 0.0, 1.0);
            ImGui::SliderFloat4("Data2", (float*)&Selected.Data.Data2, 0.0, 1.0);
            ImGui::SliderFloat4("Data3", (float*)&Selected.Data.Data3, 0.0, 1.0);
            ImGui::SliderFloat4("Data4", (float*)&Selected.Data.Data4, 0.0, 1.0);
        }
        ImGui::End();
        
        ImGui::Begin("Stats");

        ImGui::Text("frametime %f ms", Stats.FrameTime);
        ImGui::Text("draw time %f ms", Stats.MeshDrawTime);
        ImGui::Text("update time %f ms", Stats.SceneUpdateTime);
        ImGui::Text("triangles %i", Stats.TriangleCount);
        ImGui::Text("draws %i", Stats.DrawcallCount);
        ImGui::End();
        
        //Make imgui calculate internal draw structures
        ImGui::Render();

        Draw();
        
        //get clock again, compare with start
        auto End = std::chrono::system_clock::now();
        
        //convert to microseconds (integer), then back to miliseconds
        auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start);
        Stats.FrameTime = Elapsed.count() / 1000.f;
    }
}

void VulkanEngine::ImmediateSubmit(TFunction<void(VkCommandBuffer Cmd)>&& Function)
{
    VK_CHECK(vkResetFences(Device, 1, &ImmFence));
    VK_CHECK(vkResetCommandBuffer(ImmCommandBuffer, 0));
    
    VkCommandBuffer Cmd = ImmCommandBuffer;
    
    VkCommandBufferBeginInfo CmdBeginInfo = Vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(Cmd, &CmdBeginInfo));
    
    Function(Cmd);
    
    VK_CHECK(vkEndCommandBuffer(Cmd));
    
    VkCommandBufferSubmitInfo CmdInfo = Vkinit::command_buffer_submit_info(Cmd);
    VkSubmitInfo2 Submit = Vkinit::submit_info(&CmdInfo, nullptr, nullptr);
    
    // submit command buffer to the queue and execute it.
    // RenderFence will now block until the graphic commands finish execution
    
    VK_CHECK(vkQueueSubmit2(GraphicsQueue, 1, &Submit, ImmFence));
    
    VK_CHECK(vkWaitForFences(Device, 1, &ImmFence, true, UINT64_MAX));

}

void VulkanEngine::DrawImGui(VkCommandBuffer Cmd, VkImageView TargetImageView)
{
    VkRenderingAttachmentInfo ColorAttachment = Vkinit::attachment_info(TargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo RenderInfo = Vkinit::rendering_info(SwapchainExtent, &ColorAttachment, nullptr);
    
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

FGpuMeshBuffers VulkanEngine::UploadMesh(TArrayView<uint32> Indices, TArrayView<FVertex> Vertices)
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

FAllocatedImage VulkanEngine::CreateImage(VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool bMipmapped)
{
    FAllocatedImage NewImage;
    NewImage.ImageFormat = Format;
    NewImage.ImageExtent = Size;
    
    VkImageCreateInfo ImageInfo = Vkinit::ImageCreateInfo(Format, Usage, Size);
    if (bMipmapped)
    {
        ImageInfo.mipLevels = FMath::FloorToInt<uint32>(FMath::Log2(FMath::Max(Size.width, Size.height))) + 1;
    }
    
    //Always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo AllocInfo {};
    AllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    AllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    //Allocate and create the image
    VK_CHECK(vmaCreateImage(Allocator, &ImageInfo, &AllocInfo, &NewImage.Image, &NewImage.Allocation, nullptr));
    
    //if the format is depth, we will need to have it use the correct aspect flag
    VkImageAspectFlags AspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Format == VK_FORMAT_D32_SFLOAT)
    {
        AspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    
    //build a image-view for the image
    VkImageViewCreateInfo ViewInfo = Vkinit::ImageViewCreateInfo(Format, NewImage.Image, AspectFlag);
    ViewInfo.subresourceRange.levelCount = ImageInfo.mipLevels;
    
    VK_CHECK(vkCreateImageView(Device, &ViewInfo, nullptr, &NewImage.ImageView));
    
    return NewImage;
}

FAllocatedImage VulkanEngine::CreateImage(void* Data, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage,
    bool bMipmapped)
{
    size_t DataSize = Size.depth * Size.width * Size.height * 4;
    FAllocatedBuffer UploadBuffer = CreateBuffer(DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
    FMemory::Memcpy(UploadBuffer.Info.pMappedData, Data, DataSize);
    
    FAllocatedImage NewImage = CreateImage(Size, Format, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, bMipmapped);
    
    ImmediateSubmit([&](VkCommandBuffer Cmd)
    {
       vkutil::TransitionImage(Cmd, NewImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); 
        
        VkBufferImageCopy CopyRegion{};
        
        CopyRegion.bufferOffset = 0;
        CopyRegion.bufferRowLength = 0;
        CopyRegion.bufferImageHeight = 0;
        
        VkImageSubresourceLayers& Subresource = CopyRegion.imageSubresource;
        
        Subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Subresource.mipLevel = 0;
        Subresource.baseArrayLayer = 0;
        Subresource.layerCount = 1;
        CopyRegion.imageExtent = Size;
        
        // copy buffer to the image
        vkCmdCopyBufferToImage(Cmd, UploadBuffer.Buffer, NewImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);
        
        vkutil::TransitionImage(Cmd, NewImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
    });
    
    DestroyBuffer(UploadBuffer);
    return NewImage;
    
}

void VulkanEngine::DestroyImage(const FAllocatedImage& Img)
{
    vkDestroyImageView(Device, Img.ImageView, nullptr);
    vmaDestroyImage(Allocator, Img.Image, Img.Allocation);
}

void VulkanEngine::ResizeSwapChain()
{
    vkDeviceWaitIdle(Device);
    DestroySwapchain();
    
    int32 width, height;
    SDL_GetWindowSize(Window, &width, &height);
    WindowExtent.width = width;
    WindowExtent.height = height;
    
    CreateSwapchain(WindowExtent.width, WindowExtent.height);
    bResizeRequest = false;
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
    
    MetalRoughMat.BuildPipelines(*this);
}

void VulkanEngine::InitRenderable()
{
    FString StructurePath = {"Meshes/Structure.glb"};
    auto StructureFile = LoadGltfMeshes(*this, StructurePath);
    
    assert(StructureFile.has_value());
    
    LoadedScenes["Structure"] = *StructureFile;
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
    if (!vkutil::LoadShaderModule("TexImage.frag.spv", Device, TriangleFragShader))
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
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = Vkinit::PipelineLayoutCreateInfo();
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
    PipelineBuilder.DisableDepthTest();
    
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
    if (!vkutil::LoadShaderModule("TexImage.frag.spv", Device, TriangleFragShader))
    {
        fmt::println("Error when building the fragment shader");
    }
    else
    {
        fmt::println("Mesh fragment shader succesfully loaded");
    }
    
    VkShaderModule TriangleVertexShader;
    if (!vkutil::LoadShaderModule("ColoredTriangleMesh.vert.spv", Device, TriangleVertexShader))
    {
        fmt::println("Error when building the shader shader");
    }
    else
    {
        fmt::println("Mesh vertex shader succesfully loaded");
    }
    
    VkPushConstantRange BufferRange{};
    BufferRange.offset = 0;
    BufferRange.size = sizeof(FGpuDrawPushConstants);
    BufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = Vkinit::PipelineLayoutCreateInfo();
    PipelineLayoutInfo.pPushConstantRanges = &BufferRange;
    PipelineLayoutInfo.pushConstantRangeCount = 1;
    PipelineLayoutInfo.pSetLayouts = &SingleImageDescriptorLayout;
    PipelineLayoutInfo.setLayoutCount = 1;
    
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
    PipelineBuilder.EnableBlendingAdditive();
    // no depth testing
    PipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    
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

    //3 default textures, white, grey, black. 1 pixel each
    uint32 Colors[] = {
        FTransform::PackUnorm4x8(FLinearColor::White),
        FTransform::PackUnorm4x8(FLinearColor::Black),
        FTransform::PackUnorm4x8(FLinearColor::Grey)
    };
    
    WhiteImage = CreateImage(&Colors[0], VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    BlackImage = CreateImage(&Colors[1], VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    GreyImage  = CreateImage(&Colors[2], VkExtent3D{1,1,1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    
    //checkboard image
    constexpr uint32 ImageSize = 16;
    uint32 Magenta = FTransform::PackUnorm4x8(FLinearColor::Magenta);
    TStaticArray<uint32, ImageSize * ImageSize> Pixels;
    for (uint32 x = 0; x < ImageSize; ++x)
    {
        for (uint32 y = 0; y < ImageSize; ++y)
        {
            Pixels[y * ImageSize + x] = ((x & 1) ^ (y & 1)) ? Magenta : Colors[2];
        }
    }
    
    ErrorCheckerboardImage = CreateImage(Pixels.data(), VkExtent3D{ImageSize, ImageSize, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    
    VkSamplerCreateInfo SamplerInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    SamplerInfo.magFilter = VK_FILTER_NEAREST;
    SamplerInfo.minFilter = VK_FILTER_NEAREST;
    
    vkCreateSampler(Device, &SamplerInfo, nullptr, &DefaultSamplerNearest);
    
    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;
    
    vkCreateSampler(Device, &SamplerInfo, nullptr, &DefaultSamplerLinear);
    
    MainDeletionQueue.PushFunction([=, this]()
    {
        vkDestroySampler(Device, DefaultSamplerNearest, nullptr); 
        vkDestroySampler(Device, DefaultSamplerLinear, nullptr);
        
        DestroyImage(WhiteImage);
        DestroyImage(BlackImage);
        DestroyImage(GreyImage);
        
        DestroyImage(ErrorCheckerboardImage);
    });
//<<default img
    
//>> Default material    
    FGltfMetalicRoughness::FMaterialResources Resources;
    //default mat textures
    Resources.ColorImage = WhiteImage;
    Resources.ColorSampler = DefaultSamplerLinear;
    Resources.MetalRoughImage = WhiteImage;
    Resources.MetalRoughSampler = DefaultSamplerLinear;
    
    //set uniform buffer for the material data
    FAllocatedBuffer Constants = CreateBuffer(sizeof(FGltfMetalicRoughness::FMaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    
    //Write the buffer
    FGltfMetalicRoughness::FMaterialConstants* UniformData = (FGltfMetalicRoughness::FMaterialConstants*)Constants.Allocation->GetMappedData();
    UniformData->ColorFactors = FLinearColor::White;
    UniformData->MetalRoughFactors = FColor{1, 0.5, 0, 0};
    
    MainDeletionQueue.PushFunction([=, this]()
    {
        DestroyBuffer(Constants);
    });
    
    Resources.DataBuffer = Constants.Buffer;
    Resources.DataBufferOffset = 0;
    
    DefaultData = MetalRoughMat.WriteMaterial(Device, EMaterialPass::Opaque, Resources, GlobalDescriptorAllocator);
    
//<< Default material
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
        DrawImageDescLayout = Builder.Build(Device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    
    //allocate a descriptor set for our draw image
    DrawImageDescriptors = GlobalDescriptorAllocator.Allocate(Device, DrawImageDescLayout);
    FDescriptorWriter Writer;
    Writer.WriteImage(0, DrawImage.ImageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    
    Writer.UpdateSet(Device, DrawImageDescriptors);
    
    //make sure both the descriptor allocator and the new layout get cleaned up properly
	MainDeletionQueue.PushFunction([=, this]()
	{
	    GlobalDescriptorAllocator.DestroyPool(Device);
	    vkDestroyDescriptorSetLayout(Device, DrawImageDescLayout, nullptr);
	});
    
    for (FFrameData& Frame : Frames) {
        //Create a descriptor pool
        TArray<FDescriptorAllocator::FPoolSizeRation> FrameSizes{
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}
        };
        
        Frame.FrameDescriptors = FDescriptorAllocator{};
        Frame.FrameDescriptors.InitPool(Device, 1000, FrameSizes);
        
        MainDeletionQueue.PushFunction([&]()
        {
            Frame.FrameDescriptors.DestroyPool(Device);
        });
    }
    
    {
        FDescriptorLayoutBuilder Builder;
        Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        GpuSceneDataDescriptorLayout = Builder.Build(Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
 
    {
        FDescriptorLayoutBuilder Builder;
        Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        SingleImageDescriptorLayout = Builder.Build(Device, VK_SHADER_STAGE_FRAGMENT_BIT);
    } 
    
}

void VulkanEngine::InitVulkan()
{
    vkb::InstanceBuilder builder;
    
    //make instance with debug
    auto ResInstance = builder.set_app_name("Chimera Engine")
    .request_validation_layers(bUseValidationLayers)
    .use_default_debug_messenger()
    .require_api_version(1, 3, 0)
    .build();
    
    vkb::Instance VkbInstance = ResInstance.value();
    
    Instance = VkbInstance.instance;
    DebugMessenger = VkbInstance.debug_messenger;
    
    SDL_Vulkan_CreateSurface(Window, Instance, &WindowSurface);
    
    VkPhysicalDeviceVulkan13Features Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    Features.dynamicRendering = true;
    Features.synchronization2 = true;
    
    VkPhysicalDeviceVulkan12Features Features12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    Features12.bufferDeviceAddress = true;
    Features12.descriptorIndexing = true;
    
    //Select GPU
    vkb::PhysicalDeviceSelector Selector { VkbInstance };
    vkb::PhysicalDevice PhysicalDevice = Selector
        .set_minimum_version(1,3)
        .set_required_features_13(Features)
        .set_required_features_12(Features12)
        .set_surface(WindowSurface)
        .select()
        .value();
    
    vkb::DeviceBuilder DeviceBuilder { PhysicalDevice };
    vkb::Device VkbDevice = DeviceBuilder.build().value();
    
    Device = VkbDevice.device;
    ChosenGPU = PhysicalDevice.physical_device;
    
    // --------- Initialize Graphics Queue ----------------
    GraphicsQueue = VkbDevice.get_queue(vkb::QueueType::graphics).value();
    GraphicsQueueFamily = VkbDevice.get_queue_index(vkb::QueueType::graphics).value();
 
    
    //Initialize memory allocator
    VmaAllocatorCreateInfo AllocatorInfo{};
    AllocatorInfo.physicalDevice = ChosenGPU;
    AllocatorInfo.device = Device;
    AllocatorInfo.instance = Instance;
    AllocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&AllocatorInfo, &Allocator);
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
    
    VkImageCreateInfo RImgInfo = Vkinit::ImageCreateInfo(DrawImage.ImageFormat, DrawImageUsages, DrawImageExtent);
    
    //for draw image, allocate it from gpu local memory
    VmaAllocationCreateInfo RImgAllocInfo{};
    RImgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    RImgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    //allocation and creation
    vmaCreateImage(Allocator, &RImgInfo, &RImgAllocInfo, &DrawImage.Image, &DrawImage.Allocation, nullptr);
    
    //build image-view for draw image to use for rendering
    VkImageViewCreateInfo RViewInfo = Vkinit::ImageViewCreateInfo(DrawImage.ImageFormat, DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
    
    VK_CHECK(vkCreateImageView(Device, &RViewInfo, nullptr, &DrawImage.ImageView));
    
    DepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
    DepthImage.ImageExtent = DrawImageExtent;
    
    VkImageUsageFlags DepthImageUsages{};
    DepthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    VkImageCreateInfo DepthImgInfo = Vkinit::ImageCreateInfo(DepthImage.ImageFormat, DepthImageUsages, DepthImage.ImageExtent);
    
    //allocate and create the image
    vmaCreateImage(Allocator, &DepthImgInfo, &RImgAllocInfo, &DepthImage.Image, &DepthImage.Allocation, nullptr);
    
    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo DepthViewInfo = Vkinit::ImageViewCreateInfo(DepthImage.ImageFormat, DepthImage.Image, VK_IMAGE_ASPECT_DEPTH_BIT);
    
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
    VkCommandBufferAllocateInfo CmdAllocInfo = Vkinit::command_buffer_allocate_info(ImmCommandPool, 1);
    
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
    VkFenceCreateInfo FenceCreateInfo = Vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = Vkinit::semaphore_create_info();

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
    vkb::SwapchainBuilder swapchainBuilder { ChosenGPU, Device, WindowSurface };
    
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

