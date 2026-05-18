#version 450

layout(set = 0, binding = 0) uniform sampler2D gBaseColor;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;

layout(push_constant) uniform DebugPushConstants{
    uint debugViewMode;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 baseColor = texture(gBaseColor, fragUV);
    vec3 normal = normalize(texture(gNormal, fragUV).xyz);
    vec4 material = texture(gMaterial, fragUV);

    if(PC.debugViewMode == 2) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
    } else if(PC.debugViewMode == 3) {
        outColor = vec4(vec3(material.r), 1.0);
    } else if(PC.debugViewMode == 4) {
        outColor = vec4(vec3(material.g), 1.0);
    } else if(PC.debugViewMode == 5) {
        outColor = vec4(0.03, 0.05, 0.08, 1.0);
    } else {
        outColor = baseColor;
    }
}
