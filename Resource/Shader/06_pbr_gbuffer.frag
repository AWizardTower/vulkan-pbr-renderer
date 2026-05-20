#version 450

layout(push_constant) uniform PushConstants{
    mat4 mvp;
    vec4 worldRows[3];
    vec4 material;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec3 fragWorldPosition;

layout(location = 0) out vec4 outBaseColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec4 outWorldPosition;

void main() {
    vec3 normal = normalize(fragWorldNormal);
    float colorType = PC.material.x;
    float roughness = clamp(PC.material.y, 0.04, 1.0);
    float metallic = clamp(PC.material.z, 0.0, 1.0);

    vec3 normalColor = normal * 0.5 + 0.5;
    vec3 uvColor = vec3(fragUV, 0.25);
    vec3 baseColor = colorType < 0.5 ? normalColor : uvColor;

    outBaseColor = vec4(baseColor, 1.0);
    outNormal = vec4(normal, 1.0);
    outMaterial = vec4(roughness, metallic, 1.0, 1.0);
    outWorldPosition = vec4(fragWorldPosition, 1.0);
}
