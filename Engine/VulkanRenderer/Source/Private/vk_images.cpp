#include <vk_images.h>

#include "vk_initializers.h"


void vkutil::TransitionImage(VkCommandBuffer Cmd, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout)
{
    VkImageMemoryBarrier2 ImageBarrier { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    ImageBarrier.pNext = nullptr;
    
    ImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ImageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    ImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ImageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    
    ImageBarrier.oldLayout = CurrentLayout;
    ImageBarrier.newLayout = NewLayout;
    
    VkImageAspectFlags AspectMask = (NewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    ImageBarrier.subresourceRange = Vkinit::ImageSubresourceRange(AspectMask);
    ImageBarrier.image = Image;
    
    VkDependencyInfo DepInfo {};
    DepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    DepInfo.pNext = nullptr;
    
    DepInfo.imageMemoryBarrierCount = 1;
    DepInfo.pImageMemoryBarriers = &ImageBarrier;
    
    vkCmdPipelineBarrier2(Cmd, &DepInfo);
}

void vkutil::CopyImageToImage(VkCommandBuffer Cmd, VkImage Source, VkImage Dest, VkExtent2D SrcSize, VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion {.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
    
    BlitRegion.srcOffsets[1].x = SrcSize.width;
    BlitRegion.srcOffsets[1].y = SrcSize.height;
    BlitRegion.srcOffsets[1].z = 1;
    
    BlitRegion.dstOffsets[1].x = DstSize.width;
    BlitRegion.dstOffsets[1].y = DstSize.height;
    BlitRegion.dstOffsets[1].z = 1;
    
    BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    BlitRegion.srcSubresource.baseArrayLayer = 0;
    BlitRegion.srcSubresource.layerCount = 1;
    BlitRegion.srcSubresource.mipLevel = 0;
    
    BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    BlitRegion.dstSubresource.baseArrayLayer = 0;
    BlitRegion.dstSubresource.layerCount = 1;
    BlitRegion.dstSubresource.mipLevel = 0;
    
    VkBlitImageInfo2 BlitInfo { .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
    BlitInfo.dstImage = Dest;
    BlitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    
    BlitInfo.srcImage = Source;
    BlitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    
    BlitInfo.filter = VK_FILTER_LINEAR;
    BlitInfo.regionCount = 1;
    BlitInfo.pRegions = &BlitRegion;
    
    vkCmdBlitImage2(Cmd, &BlitInfo);
}

void vkutil::GenerateMipmaps(VkCommandBuffer Cmd, VkImage Image, VkExtent2D ImageSize)
{
    int MipLevels = FMath::FloorToInt<int>(FMath::Log2(FMath::Max(ImageSize.width, ImageSize.height))) + 1;
    for (int32 Mip = 0; Mip < MipLevels; Mip++)
    {
        
        VkExtent2D HalfSize = ImageSize;
        HalfSize.width /= 2;
        HalfSize.height /= 2;
        
        VkImageMemoryBarrier2 ImageBarrier{ .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .pNext = nullptr};
        
        ImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ImageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        ImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ImageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        
        ImageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        ImageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        
        VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageBarrier.subresourceRange = Vkinit::ImageSubresourceRange(AspectMask);
        ImageBarrier.subresourceRange.levelCount = 1;
        ImageBarrier.subresourceRange.baseMipLevel = Mip;
        ImageBarrier.image = Image;
        
        VkDependencyInfo DependencyInfo { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
        DependencyInfo.imageMemoryBarrierCount = 1;
        DependencyInfo.pImageMemoryBarriers = &ImageBarrier;
        
        vkCmdPipelineBarrier2(Cmd, &DependencyInfo);
        
        //not the last level
        if (Mip < MipLevels - 1)
        {
            VkImageBlit2 BlitRegion { .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
            
            BlitRegion.srcOffsets[1].x = ImageSize.width;
            BlitRegion.srcOffsets[1].y = ImageSize.height;
            BlitRegion.srcOffsets[1].z = 1;
            
            BlitRegion.dstOffsets[1].x = HalfSize.width;
            BlitRegion.dstOffsets[1].y = HalfSize.height;
            BlitRegion.dstOffsets[1].z = 1;
            
            BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            BlitRegion.srcSubresource.baseArrayLayer = 0;
            BlitRegion.srcSubresource.layerCount = 1;
            BlitRegion.srcSubresource.mipLevel = Mip;
            
            BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            BlitRegion.dstSubresource.baseArrayLayer = 0;
            BlitRegion.dstSubresource.layerCount = 1;
            BlitRegion.dstSubresource.mipLevel = Mip + 1;
            
            VkBlitImageInfo2 BlitInfo { .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
            BlitInfo.dstImage = Image;
            BlitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            BlitInfo.srcImage = Image;
            BlitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            BlitInfo.filter = VK_FILTER_LINEAR;
            BlitInfo.regionCount = 1;
            BlitInfo.pRegions = &BlitRegion;
            
            vkCmdBlitImage2(Cmd, &BlitInfo);
            
            ImageSize = HalfSize;
            
        }
    }
    
    // transition all mip levels into the final read_only layout
    TransitionImage(Cmd, Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
}
