#version 450

layout(location = 0) in vec3 a_Pos;

layout(push_constant) uniform PushConstants{
    mat4 lightMVP;
} PC;

void main() {
    gl_Position = PC.lightMVP * vec4(a_Pos, 1.0);
}
