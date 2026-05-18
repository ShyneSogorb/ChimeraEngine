#version 450

//shader input
layout (location = 0) in vec3 inColor;

//shader output
layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(inColor, 0.5f);
}