//
// Created by user on 13/05/2026.
//

#pragma once

#include <vk_types.h>

// Forward declaration de SDL_Event (es una unión en SDL2)
union SDL_Event;

class FCamera
{
public:
    
    FVector Velocity;
    FVector Position;
    
    //vertical rotation
    float Pitch;
    //horizontal rotation
    float Yaw;
    float Speed{ .1f };
    float Sensitivity{ 1.f / 250 };
    
    FMatrix GetViewMatrix() const;
    FMatrix GetRotationMatrix() const;
    
    void ProcessSdlEvents(SDL_Event& Event);
    
    void Update(float DeltaTime = 1);
    
    void Reset();
    
};
