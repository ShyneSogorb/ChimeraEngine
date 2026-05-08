// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_descriptors.h>
#include <vk_types.h>

struct FMeshAsset;

struct FFrameData
{
	VkCommandPool CommandPool{ nullptr };
	VkCommandBuffer MainCommandBuffer{ nullptr };
	
	//Semophore
	VkSemaphore SwapchainSemaphore { nullptr };
	VkSemaphore RenderSemaphore { nullptr };
	VkFence RenderFence { nullptr };
	
	FDeletionQueue DeletionQueue;
	
};

struct FComputePushConstants
{
	union
	{
		struct {
			FVector4 Data1;
			FVector4 Data2;
			FVector4 Data3;
			FVector4 Data4;
		};
		FVector4 Data[4];
	};
};

struct FComputeEffect
{
	const char* Name{};
	
	VkPipeline Pipeline{};
	VkPipelineLayout Layout{};
	
	FComputePushConstants Data{};
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:

	bool bIsInitialized{ false };
	int FrameNumber {0};
	bool bStopRendering{ false };
	VkExtent2D WindowExtent{ 1700 , 900 };

	struct SDL_Window* Window{ nullptr };

	static VulkanEngine& Get();
	
	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void Draw();

	//run main loop
	void run();
	
	// --- omitted ---
	
	//library handle
	VkInstance Instance{ nullptr };
	
	//Debug output handle
	VkDebugUtilsMessengerEXT DebugMessenger{ nullptr };
	
	//GPU device
	VkPhysicalDevice ChosenGPU{ nullptr };
	
	// Command device
	VkDevice Device{ nullptr };
	
	//Window surface
	VkSurfaceKHR Surface{ nullptr };
	
	VkSwapchainKHR Swapchain{ nullptr };
	VkFormat SwapchainImageFormat {VK_FORMAT_B8G8R8A8_UNORM};
	
	TArray<VkImage> SwapchainImages;
	TArray<VkImageView> SwapchainImageViews;
	VkExtent2D SwapchainExtent{ 1920, 1080 };
	
	//FrameData
	
	FFrameData Frames[FRAME_OVERLAP];
	
	FFrameData& GetCurrentFrame		(this VulkanEngine& Self) { return Self.Frames[(Self.FrameNumber	)	% FRAME_OVERLAP]; }
	FFrameData& GetNextFrame		(this VulkanEngine& Self) { return Self.Frames[(Self.FrameNumber + 1)	% FRAME_OVERLAP]; }
	FFrameData& GetPreviousFrame	(this VulkanEngine& Self) { return Self.Frames[(Self.FrameNumber - 1)	% FRAME_OVERLAP]; }
	
	VkQueue GraphicsQueue{ nullptr };
	uint32_t GraphicsQueueFamily { 0 };
	
	FDeletionQueue MainDeletionQueue;
	VmaAllocator Allocator { nullptr };
	
	FAllocatedImage DrawImage {nullptr};
	FAllocatedImage DepthImage {nullptr};
	
	VkExtent2D DrawExtent{ 1920, 1080 };
	
	FDescriptorAllocator GlobalDescriptorAllocator{};
	
	VkDescriptorSet DrawImageDescriptors{};
	VkDescriptorSetLayout DrawImageDescLayout{};
	
	VkPipeline GradientPipeline{};
	VkPipelineLayout GradientPipelineLayout{};
	
	VkFence ImmFence{};
	VkCommandBuffer ImmCommandBuffer{ nullptr };
	VkCommandPool ImmCommandPool{ nullptr };
	
	TArray<FComputeEffect> BackgroundEffects{};
	int CurrentBackgroundEffect { 0 };
	
	void ImmediateSubmit(std::function<void(VkCommandBuffer Cmd)>&& Function);
	
	void DrawImGui(VkCommandBuffer Cmd, VkImageView TargetImageView);
	
	VkPipelineLayout TrianglePipelineLayout{nullptr};
	VkPipeline TrianglePipeline{};
	
	FAllocatedBuffer CreateBuffer(size_t AllocSize, VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage);
	void DestroyBuffer(const FAllocatedBuffer& Buffer);
	
	FGpuMeshBuffers UploadMesh(std::span<uint32> Indices, std::span<FVertex> Vertices);
	
	VkPipelineLayout MeshPipelineLayout{nullptr};
	VkPipeline MeshPipeline{nullptr};
	
	FGpuMeshBuffers Rectangle;
	
	TArray<TSharedPtr<FMeshAsset>> TestMeshes;
	
private:
	
	
	void InitImgUi();
	void InitPipelines();
	void InitBackgroundPipelines();
	void InitTrianglePipeline();
	void InitMeshPipeline();
	void InitDefaultData();
	
	void InitDescriptors();
	
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	
	void DrawBackground(VkCommandBuffer Cmd);
	void DrawGeometry(VkCommandBuffer Cmd);
};
