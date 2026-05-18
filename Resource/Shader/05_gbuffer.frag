#version 450

layout(push_constant) uniform PushConstants{
    mat4 matrix;
    vec4 normalCols[3];
    vec4 params;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragWorldNormal;

layout(location = 0) out vec4 outBaseColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

void main() {
    vec3 normal = normalize(fragWorldNormal);
    float colorType = PC.params.x;
    float roughness = PC.params.y;
    float metallic = PC.params.z;

    vec3 normalColor = normal * 0.5 + 0.5;
    vec3 uvColor = vec3(fragUV, 0.25);
    outBaseColor = vec4(colorType < 0.5 ? normalColor : uvColor, 1.0);
    outNormal = vec4(normal, 1.0);
    outMaterial = vec4(roughness, metallic, 0.0, 1.0);
}
