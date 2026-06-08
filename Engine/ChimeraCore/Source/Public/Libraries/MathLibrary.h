//
// Created by user on 14/05/2026.
//

#pragma once

#include "cmath"

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
    template <typename T, typename ...U>
    constexpr T Min(U... Values)
    {
        return (T)std::min({Values...});
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
        return (T)std::sin((double)Value);
    }
    
    template <typename T>
    constexpr T Cos(T Value)
    {
        return (T)std::cos((double)Value);
    }
    
    template <typename T>
    constexpr T Floor(T Value)
    {
        return (T)std::floor((double)Value);
    }
    
    template <typename T, typename U>
    constexpr T FloorToInt(U Value)
    {
        return (T)std::floor((double)Value);
    }
    
    template <typename T>
    constexpr T Ceil(T Value)
    {        
        return (T)std::ceil(Value);
    }
    
    template <typename T, typename U>
    constexpr T CeilToInt(U Value)
    {
        return (T)std::ceil(Value);
    }
    
    template <typename T>
    constexpr T Log2(T Value)
    {
        return (T)std::log2(static_cast<double>(Value));
    }
    
}