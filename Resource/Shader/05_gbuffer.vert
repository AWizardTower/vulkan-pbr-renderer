#version 450

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_Texcoord;
layout(location = 2) in vec3 a_Normal;

layout(push_constant) uniform PushConstants{
    mat4 matrix;
    vec4 normalCols[3];
    vec4 params;
} PC;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragWorldNormal;

void main() {
    mat3 normalMatrix = mat3(PC.normalCols[0].xyz, PC.normalCols[1].xyz, PC.normalCols[2].xyz);
    fragUV = a_Texcoord;
    fragWorldNormal = normalize(normalMatrix * a_Normal);
    gl_Position = PC.matrix * vec4(a_Pos, 1.0);
}
