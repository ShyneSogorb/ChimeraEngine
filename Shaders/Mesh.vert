#version 450

#extension GL_GOOGLE_include_directive : require 
#extension GL_EXT_buffer_reference : require 

#include "InputStructures.glsl"

layout (location = 0) out vec3 OutNormal;
layout (location = 1) out vec3 OutColor;
layout (location = 2) out vec2 OutUV;

struct FVertex{
    vec3 Position;
    float UVx;
    vec3 Normal;
    float UVy;
    vec4 Color;
};

layout(buffer_reference, std430) readonly buffer FVertexBuffer{
    FVertex Vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
        mat4 RenderMatrix;
        FVertexBuffer VertexBuffer;
} PushConstants;

void main()
{
        FVertex Vert = PushConstants.VertexBuffer.Vertices[gl_VertexIndex];
        
        vec4 Position = vec4(Vert.Position, 1.0f);
        
        gl_Position = SceneData.ViewProj * PushConstants.RenderMatrix * Position;
        
        OutNormal = (PushConstants.RenderMatrix * vec4(Vert.Normal, 0.f)).xyz;
        OutColor = Vert.Color.xyz * MaterialData.ColorFactor.xyz;
        OutUV = vec2(Vert.UVx, Vert.UVy);
}