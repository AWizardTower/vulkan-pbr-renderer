#version 450

layout(set = 0, binding = 0) uniform sampler2D lightingColor;
layout(set = 0, binding = 1) uniform sampler2D gBaseColor;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gMaterial;
layout(set = 0, binding = 4) uniform sampler2D gWorldPosition;
layout(set = 0, binding = 5) uniform sampler2D shadowDepth;

layout(push_constant) uniform ShadowDebugPushConstants{
    mat4 lightViewProj;
    vec4 shadowParams;
    vec4 debugParams;
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

vec3 MetallicDebugColor(float metallic) {
    vec3 dielectric = vec3(0.08);
    vec3 metal = vec3(1.0, 0.88, 0.18);
    return mix(dielectric, metal, metallic);
}

float SampleShadowFactor(vec3 worldPosition) {
    vec4 shadowClip = PC.lightViewProj * vec4(worldPosition, 1.0);
    if(shadowClip.w <= 0.0) {
        return 1.0;
    }

    vec3 shadowNdc = shadowClip.xyz / shadowClip.w;
    vec2 shadowUV = shadowNdc.xy * 0.5 + 0.5;
    float receiverDepth = shadowNdc.z;

    if(shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
       shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
       receiverDepth < 0.0 || receiverDepth > 1.0) {
        return 1.0;
    }

    float texelSize = PC.shadowParams.x;
    float depthBias = PC.shadowParams.y;
    float visibility = 0.0;
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float casterDepth = texture(shadowDepth, shadowUV + offset).r;
            visibility += receiverDepth - depthBias <= casterDepth ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

void main() {
    uint debugViewMode = uint(PC.debugParams.x + 0.5);
    vec3 lit = texture(lightingColor, fragUV).rgb;
    vec3 baseColor = texture(gBaseColor, fragUV).rgb;
    vec3 normal = normalize(texture(gNormal, fragUV).xyz);
    vec4 material = texture(gMaterial, fragUV);
    vec3 worldPosition = texture(gWorldPosition, fragUV).xyz;

    if(debugViewMode == 2) {
        outColor = vec4(vec3(texture(shadowDepth, fragUV).r), 1.0);
    } else if(debugViewMode == 3) {
        outColor = vec4(vec3(SampleShadowFactor(worldPosition)), 1.0);
    } else if(debugViewMode == 4) {
        outColor = vec4(baseColor, 1.0);
    } else if(debugViewMode == 5) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
    } else if(debugViewMode == 6) {
        outColor = vec4(mix(baseColor * 0.18, RoughnessHeatmap(material.r), 0.9), 1.0);
    } else if(debugViewMode == 7) {
        outColor = vec4(MetallicDebugColor(material.g), 1.0);
    } else {
        outColor = vec4(ToneMap(lit), 1.0);
    }
}
