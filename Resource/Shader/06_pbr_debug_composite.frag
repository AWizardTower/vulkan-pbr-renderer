#version 450

layout(set = 0, binding = 0) uniform sampler2D lightingColor;
layout(set = 0, binding = 1) uniform sampler2D gBaseColor;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gMaterial;

layout(push_constant) uniform DebugPushConstants{
    uint debugViewMode;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

vec3 ToneMap(vec3 color) {
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

vec3 RoughnessHeatmap(float roughness) {
    vec3 glossy = vec3(0.05, 0.25, 1.0);
    vec3 mid = vec3(0.1, 0.9, 0.35);
    vec3 matte = vec3(1.0, 0.25, 0.02);
    return roughness < 0.5
        ? mix(glossy, mid, roughness * 2.0)
        : mix(mid, matte, (roughness - 0.5) * 2.0);
}

vec3 MetallicDebugColor(vec3 baseColor, float metallic) {
    vec3 dielectric = vec3(0.04, 0.045, 0.05);
    vec3 metal = mix(vec3(0.2, 0.55, 1.0), baseColor, 0.45) + vec3(0.08);
    return mix(dielectric, metal, metallic);
}

void main() {
    vec3 lit = texture(lightingColor, fragUV).rgb;
    vec3 baseColor = texture(gBaseColor, fragUV).rgb;
    vec3 normal = normalize(texture(gNormal, fragUV).xyz);
    vec4 material = texture(gMaterial, fragUV);

    if(PC.debugViewMode == 2) {
        outColor = vec4(baseColor, 1.0);
    } else if(PC.debugViewMode == 3) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
    } else if(PC.debugViewMode == 4) {
        outColor = vec4(mix(baseColor * 0.18, RoughnessHeatmap(material.r), 0.9), 1.0);
    } else if(PC.debugViewMode == 5) {
        outColor = vec4(MetallicDebugColor(baseColor, material.g), 1.0);
    } else {
        outColor = vec4(ToneMap(lit), 1.0);
    }
}
