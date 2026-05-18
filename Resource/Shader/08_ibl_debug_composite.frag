#version 450

layout(set = 0, binding = 0) uniform sampler2D lightingColor;
layout(set = 0, binding = 1) uniform sampler2D gBaseColor;
layout(set = 0, binding = 2) uniform sampler2D gNormal;
layout(set = 0, binding = 3) uniform sampler2D gMaterial;
layout(set = 0, binding = 4) uniform sampler2D gWorldPosition;
layout(set = 0, binding = 5) uniform sampler2D shadowDepth;
layout(set = 0, binding = 6) uniform sampler2D environmentMap;

layout(set = 0, binding = 7) uniform IBLDebugSettings{
    mat4 inverseViewProj;
    mat4 lightViewProj;
    vec4 cameraPosition;
    vec4 debugParams;
    vec4 shadowParams;
    vec4 iblParams;
} Settings;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 ToneMap(vec3 color) {
    float exposure = max(Settings.debugParams.y, 0.001);
    float gammaValue = max(Settings.debugParams.z, 0.001);
    color = vec3(1.0) - exp(-color * exposure);
    return pow(color, vec3(1.0 / gammaValue));
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec2 DirectionToEquirectUV(vec3 dir) {
    dir = normalize(dir);
    float phi = atan(dir.z, dir.x);
    float theta = asin(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, 0.5 - theta / PI);
}

vec3 SampleEnvironment(vec3 dir, float lod) {
    return max(textureLod(environmentMap, DirectionToEquirectUV(dir), lod).rgb, vec3(0.0));
}

vec3 ViewRayFromUV(vec2 uv) {
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 nearWorld = Settings.inverseViewProj * vec4(ndc, 0.0, 1.0);
    vec4 farWorld = Settings.inverseViewProj * vec4(ndc, 1.0, 1.0);
    nearWorld.xyz /= max(nearWorld.w, 0.0001);
    farWorld.xyz /= max(farWorld.w, 0.0001);
    return normalize(farWorld.xyz - nearWorld.xyz);
}

float SampleShadowFactor(vec3 worldPosition) {
    vec4 shadowClip = Settings.lightViewProj * vec4(worldPosition, 1.0);
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

    float texelSize = Settings.shadowParams.x;
    float depthBias = Settings.shadowParams.y;
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

vec3 RoughnessHeatmap(float roughness) {
    vec3 glossy = vec3(0.05, 0.25, 1.0);
    vec3 mid = vec3(0.1, 0.9, 0.35);
    vec3 matte = vec3(1.0, 0.25, 0.02);
    return roughness < 0.5
        ? mix(glossy, mid, roughness * 2.0)
        : mix(mid, matte, (roughness - 0.5) * 2.0);
}

vec3 MetallicDebugColor(float metallic) {
    return mix(vec3(0.08), vec3(1.0, 0.88, 0.18), metallic);
}

void main() {
    uint debugViewMode = uint(Settings.debugParams.x + 0.5);
    vec3 viewRay = ViewRayFromUV(fragUV);
    float maxEnvMip = max(float(textureQueryLevels(environmentMap) - 1), 0.0);
    vec3 skyColor = SampleEnvironment(viewRay, 0.0);

    vec4 worldPositionSample = texture(gWorldPosition, fragUV);
    bool hasGeometry = worldPositionSample.a > 0.5;

    if(debugViewMode == 2) {
        outColor = vec4(ToneMap(skyColor), 1.0);
        return;
    }

    if(!hasGeometry) {
        outColor = debugViewMode == 1 ? vec4(ToneMap(skyColor), 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 baseColor = texture(gBaseColor, fragUV).rgb;
    vec3 N = normalize(texture(gNormal, fragUV).xyz);
    vec4 material = texture(gMaterial, fragUV);
    vec3 worldPosition = worldPositionSample.xyz;
    float roughness = clamp(material.r, 0.04, 1.0);
    float metallic = clamp(material.g, 0.0, 1.0);
    vec3 V = normalize(Settings.cameraPosition.xyz - worldPosition);
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 envDiffuse = SampleEnvironment(N, maxEnvMip) * baseColor * (1.0 - metallic) * Settings.iblParams.x;
    vec3 envSpecular = SampleEnvironment(reflect(-V, N), roughness * maxEnvMip) * F * Settings.iblParams.y;

    if(debugViewMode == 3) {
        outColor = vec4(ToneMap(envDiffuse), 1.0);
    } else if(debugViewMode == 4) {
        outColor = vec4(ToneMap(envSpecular), 1.0);
    } else if(debugViewMode == 5) {
        outColor = vec4(vec3(SampleShadowFactor(worldPosition)), 1.0);
    } else if(debugViewMode == 6) {
        outColor = vec4(baseColor, 1.0);
    } else if(debugViewMode == 7) {
        outColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if(debugViewMode == 8) {
        outColor = vec4(RoughnessHeatmap(roughness), 1.0);
    } else if(debugViewMode == 9) {
        outColor = vec4(MetallicDebugColor(metallic), 1.0);
    } else {
        outColor = vec4(ToneMap(texture(lightingColor, fragUV).rgb), 1.0);
    }
}
