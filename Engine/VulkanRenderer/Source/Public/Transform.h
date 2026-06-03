//
// Created by user on 14/05/2026.
//

#pragma once
#include "Types/Types.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

struct FTransform
{
    
    static uint32 PackUnorm4x8(FVector4 const& v)
    {
        return glm::packUnorm4x8(v);
    }
    
    static FMatrix Translate(FMatrix const& m, FVector const& v)
    {
        return glm::translate(m, v);
    }
    
    static FMatrix Translate(FVector const& v)
    {
        return glm::translate(v);
    }
    
    static FMatrix Inverse(FMatrix const& m)
    {
        return glm::inverse(m);
    }
    
    static FQuat AngleAxis(float const& angle, FVector const& v)
    {
        return glm::angleAxis(angle, v);
    }
    
    static FMatrix ToMatrix(FQuat const& q)
    {
        return glm::toMat4(q);
    }
    
    template <typename T>
    static FMatrix Perspective(T fovy, T aspect, T zNear, T zFar)
    {
        return glm::perspective(fovy, aspect, zNear, zFar);
    }
    
    static float Radians(float degrees)
    {
        return glm::radians(degrees);
    }
    
    static FMatrix Scale(FVector const& v)
    {
        return glm::scale(v);
    }
    
    static FMatrix Scale(FMatrix m, FVector const& v)
    {
        return glm::scale(m, v);
    }
    
};
