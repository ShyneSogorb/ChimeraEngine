#version 450

#extension GL_GOOGLE_include_directive : require
#include "InputStructures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 FragColor;

void main()
{
    float LightIntensity = max(dot(inNormal, SceneData.SunlightDirection.xyz), 0.1f);
    
    vec3 Color = inColor * texture(ColorTexture, inUV).xyz;
    vec3 Ambient = Color * SceneData.AmbientColor.xyz;
    
    FragColor = vec4(Color * LightIntensity * SceneData.SunlightColor.w + Ambient, 1.f);
}