#version 450

//shader input
layout (Location = 0) in vec3 inColor;

//shader output
layout (Location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(inColor, 1.0f);
}