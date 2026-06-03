#pragma once
#include <cstdint>

// Handles opacos — el exterior no sabe qué hay dentro
struct MeshHandle
{
    uint32_t id = UINT32_MAX;
    bool IsValid() const { return id != UINT32_MAX; }
};

struct TextureHandle
{
    uint32_t id = UINT32_MAX;
    bool IsValid() const { return id != UINT32_MAX; }
};