
#include "Camera.h"

#include "Transform.h"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

FMatrix FCamera::GetViewMatrix() const
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    FMatrix Translation = FTransform::Translate(FMatrix(1.f), Position);
    FMatrix Rotation = GetRotationMatrix();
    return FTransform::Inverse(Translation * Rotation);
}

FMatrix FCamera::GetRotationMatrix() const
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    FQuat PitchRotation = FTransform::AngleAxis(Pitch, FVector{1.f, 0.f, 0.f});
    FQuat YawRotation = FTransform::AngleAxis(Yaw, FVector{0.f, -1.f, 0.f});

    return FTransform::ToMatrix(YawRotation) * FTransform::ToMatrix(PitchRotation);
}

void FCamera::ProcessSdlEvents(SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { Velocity.z = -1; }
        if (e.key.keysym.sym == SDLK_s) { Velocity.z = 1; }
        if (e.key.keysym.sym == SDLK_a) { Velocity.x = -1; }
        if (e.key.keysym.sym == SDLK_d) { Velocity.x = 1; }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) { Velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { Velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { Velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { Velocity.x = 0; }
    }

    if (e.type == SDL_MOUSEMOTION) {
        Yaw += (float)e.motion.xrel * Sensitivity;
        Pitch -= (float)e.motion.yrel * Sensitivity;
        
        //clamp pitch to avoid gimbal lock
        if (Pitch > 89.f) Pitch = 89.f;
        if (Pitch < -89.f) Pitch = -89.f;
    }
}

void FCamera::Update(float DeltaTime)
{
    FMatrix Rotation = GetRotationMatrix();
    Position += FVector(Rotation * FVector4(Velocity * Speed * DeltaTime, 0));
}

void FCamera::Reset()
{
    Position = FVector(30, 0, -85);
    Velocity = FVector(0, 0, 0);
    Pitch = 0;
    Yaw = 0;
}
