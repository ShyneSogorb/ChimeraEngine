//
// Created by user on 14/05/2026.
//

#pragma once

#include <algorithm>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "glm/fwd.hpp"

using FVector4 = glm::vec4;
using FVector3 = glm::vec3;
using FVector2 = glm::vec2;
using FVector = FVector3;
using FVector3f = glm::vec3;
using FVector2f = glm::vec2;
using FVector4f = glm::vec4;

using FQuat = glm::quat;

#define FORWARD_VECTOR { 1.f, 0.f, 0.f}

using FMatrix4 = glm::mat4;
using FMatrix = FMatrix4;

namespace FMath
{
    constexpr auto Min(auto... Values)
    {
        return std::min({Values...});
    }
    
    constexpr auto Max(auto... Values)
    {
        return std::max({Values...});
    }
    
    template <typename T>
    constexpr T Abs(T Value)
    {
        return Value < 0 ? -Value : Value;
    }
    
    template <typename T>
    constexpr T Clamp(T Value, T Min, T Max)
    {
        return std::max(Min, std::min(Value, Max));
    }
    
    template <typename T>
    constexpr T Sin(T Value)
    {
        return std::sin(Value);
    }
    
    template <typename T>
    constexpr T Cos(T Value)
    {
        return std::cos(Value);
    }
    
    template <typename T>
    constexpr T Floor(T Value)
    {
        return std::floor(Value);
    }
    
    template <typename T, typename U>
    constexpr T FloorToInt(U Value)
    {
        return std::floor(Value);
    }
    
    template <typename T>
    constexpr T Ceil(T Value)
    {        
        return std::ceil(Value);
    }
    
    template <typename T, typename U>
    constexpr T CeilToInt(U Value)
    {
        return std::ceil(Value);
    }
    
    template <typename T>
    constexpr T Log2(T Value)
    {
        return std::log2(Value);
    }
    
}