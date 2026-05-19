// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_descriptors.h>
#include <vk_types.h>

#include "Camera.h"

struct FMeshAsset;

struct FFrameData
{
	//Semophore
	VkSemaphore SwapchainSemaphore, RenderSemaphore { nullptr };
	VkFence RenderFence { nullptr };
	
	VkCommandPool CommandPool{ nullptr };
	VkCommandBuffer MainCommandBuffer{ nullptr };
	
	FDeletionQueue DeletionQueue;
	FDescriptorAllocator FrameDescriptors;
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

struct FEngineStats
{
	float FrameTime;
	int32 TriangleCount;
	int32 DrawcallCount;
	float SceneUpdateTime;
	float MeshDrawTime;
};

struct FGltfMetalicRoughness
{
	FMaterialPipeline OpaquePipeline;
	FMaterialPipeline TransparentPipeline;
	
	VkDescriptorSetLayout MaterialLayout{};
	
	struct FMaterialConstants
	{
		FColor ColorFactors;
		FColor MetalRoughFactors;
		//padding we need it anyway for uniform buffers
		char Padding[256 - sizeof(ColorFactors) - sizeof(MetalRoughFactors)];
	};
	
	struct FMaterialResources
	{
		FAllocatedImage ColorImage;
		VkSampler ColorSampler;
		FAllocatedImage MetalRoughImage;
		VkSampler MetalRoughSampler;
		VkBuffer DataBuffer;
		uint32 DataBufferOffset;
	};
	
	FDescriptorWriter Writer;
	
	void BuildPipelines(class VulkanEngine& Engine);	
	void ClearResources(VkDevice Device);
	
	FMaterialInstance WriteMaterial(VkDevice, EMaterialPass Pass, const FMaterialResources& Resources, FDescriptorAllocator& DescriptorAllocator);
	
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:

	bool bIsInitialized{ false };
	int FrameNumber {0};
	bool bStopRendering{ false };
	VkExtent2D WindowExtent{ 1920 , 1080 };

	struct SDL_Window* Window{ nullptr };

	static VulkanEngine& Get();
	
	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();
	
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
	VkSurfaceKHR WindowSurface{ nullptr };
	
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
	float RenderScale{ 1.0f };
	
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
	
	FGpuSceneData SceneData;
	
	VkDescriptorSetLayout GpuSceneDataDescriptorLayout{ nullptr };
	
	bool bResizeRequest{ false };
	
	FAllocatedImage CreateImage(VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool bMipmapped = false);
	FAllocatedImage CreateImage(void* Data, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool bMipmapped = false);
	void DestroyImage(const FAllocatedImage& Img);
	
	FAllocatedImage ErrorCheckerboardImage{ nullptr };
	FAllocatedImage WhiteImage{ nullptr };
	FAllocatedImage BlackImage{ nullptr };
	FAllocatedImage GreyImage{ nullptr };

	VkSampler DefaultSamplerLinear;
	VkSampler DefaultSamplerNearest;
	
	VkDescriptorSetLayout SingleImageDescriptorLayout{ nullptr };
	
	FMaterialInstance DefaultData;
	FGltfMetalicRoughness MetalRoughMat;
	
	FDrawContext MainDrawContext;
	TMap<FString, TSharedPtr<struct FLoadedGltf>> LoadedScenes;
	
	FEngineStats Stats;
	
private:
	
	void ResizeSwapChain();
	void InitImgUi();
	void InitPipelines();
	void InitRenderable();
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
	

	


	TMap<FString, TSharedPtr<FNode>> LoadedNodes;
	
	void UpdateScene();
	
	FCamera MainCamera;
	
};

struct FMeshNode : public FNode
{
	TSharedPtr<FMeshAsset> Mesh;
	
	virtual void Draw(const FMatrix& TopMatrix, FDrawContext& Ctx) override;
};
