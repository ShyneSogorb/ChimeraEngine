//
// Created by user on 26/05/2026.
//

#include "VulkanRenderer.h"

#include "Engine.h"
#include "vk_engine.h"

void MVulkanRenderer::ModuleStartup()
{
    Renderer = VulkanEngine::Create();
}

void MVulkanRenderer::ModuleShutdown()
{
    
}
