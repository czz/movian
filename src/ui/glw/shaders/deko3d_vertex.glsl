#version 460

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec4 a_color;

layout(location = 0) out vec2 v_texcoord;
layout(location = 1) out vec4 v_color;

layout(std140, binding = 0) uniform Uniforms {
    mat4 u_modelview;
    mat4 u_projection;
};

void main()
{
    gl_Position = u_projection * u_modelview * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
