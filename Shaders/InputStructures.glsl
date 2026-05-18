
layout(set = 0, binding = 0) uniform FSceneData{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 AmbientColor;
    vec4 SunlightDirection; //w for intensity
    vec4 SunlightColor;
} SceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{
    vec4 ColorFactor;
    vec4 MetalRoughFactor;
} MaterialData;

layout(set = 1, binding = 1) uniform sampler2D ColorTexture;
layout(set = 1, binding = 2) uniform sampler2D MetalRoughTexture;