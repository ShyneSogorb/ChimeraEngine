//
// Created by user on 13/05/2026.
//

#pragma once

#include <vk_types.h>
#include <SDL_events.h>

class FCamera
{
public:
    
    FVector Velocity;
    FVector Position;
    
    //vertical rotation
    float Pitch;
    //horizontal rotation
    float Yaw;
    float Speed{ .1 };
    float Sensitivity{ 1.f / 250 };
    
    FMatrix GetViewMatrix() const;
    FMatrix GetRotationMatrix() const;
    
    void ProcessSdlEvents(SDL_Event& Event);
    
    void Update(float DeltaTime = 1);
    
    void Reset();
    
};
