#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;

//output write
layout (location = 0) out vec4 FragColor;

//texture to access
layout (binding = 0) uniform sampler2D DisplayTexture;

void main()
{
    FragColor = texture(DisplayTexture, inUV);
}