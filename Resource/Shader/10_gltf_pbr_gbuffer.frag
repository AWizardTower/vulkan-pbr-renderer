#version 450

layout(set = 0, binding = 0) uniform MaterialParams{
    vec4 baseColorFactor;
    vec4 pbrParams;
    vec4 featureFlags;
} Material;

layout(set = 0, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 3) uniform sampler2D normalTexture;
layout(set = 0, binding = 4) uniform sampler2D occlusionTexture;

layout(push_constant) uniform PushConstants{
    mat4 mvp;
    vec4 worldRows[3];
    vec4 debugFeatureFlags;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec3 fragWorldPosition;
layout(location = 3) in vec4 fragWorldTangent;

layout(location = 0) out vec4 outBaseColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec4 outWorldPosition;

void main() {
    vec3 N = normalize(fragWorldNormal);
    vec3 T = normalize(fragWorldTangent.xyz);

    if(Material.featureFlags.x > 0.5 && PC.debugFeatureFlags.x > 0.5) {
        T = normalize(T - N * dot(N, T));
        vec3 B = normalize(cross(N, T) * fragWorldTangent.w);
        vec3 tangentNormal = texture(normalTexture, fragUV).xyz * 2.0 - 1.0;
        tangentNormal.xy *= Material.pbrParams.z;
        N = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
    }

    vec4 baseColor = texture(baseColorTexture, fragUV) * Material.baseColorFactor;
    vec4 mr = texture(metallicRoughnessTexture, fragUV);
    float roughness = clamp(mr.g * Material.pbrParams.y, 0.04, 1.0);
    float metallic = clamp(mr.b * Material.pbrParams.x, 0.0, 1.0);
    float occlusion = texture(occlusionTexture, fragUV).r;
    float ao = clamp(1.0 + Material.pbrParams.w * (occlusion - 1.0), 0.0, 1.0);
    if(PC.debugFeatureFlags.y < 0.5) {
        ao = 1.0;
    }

    outBaseColor = vec4(baseColor.rgb, 1.0);
    outNormal = vec4(N, 1.0);
    outMaterial = vec4(roughness, metallic, ao, 1.0);
    outWorldPosition = vec4(fragWorldPosition, 1.0);
}
