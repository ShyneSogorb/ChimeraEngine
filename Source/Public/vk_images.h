
#pragma once 

namespace vkutil {

    void TransitionImage(VkCommandBuffer Cmd, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout);
    
    void CopyImageToImage(VkCommandBuffer Cmd, VkImage Source, VkImage Dest, VkExtent2D SrcSize, VkExtent2D DstSize);

};