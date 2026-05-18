#version 450

layout(set = 0, binding = 0) uniform sampler2D gBaseColor;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gWorldPosition;

layout(push_constant) uniform LightPushConstants{
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambient;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH = max(dot(N, H), 0.0);
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

float GeometrySchlickGGX(float NoV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NoV / max(NoV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NoV, roughness) * GeometrySchlickGGX(NoL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 baseColor = texture(gBaseColor, fragUV).rgb;
    vec3 N = normalize(texture(gNormal, fragUV).xyz);
    vec4 material = texture(gMaterial, fragUV);
    vec3 worldPosition = texture(gWorldPosition, fragUV).xyz;

    float roughness = clamp(material.r, 0.04, 1.0);
    float metallic = clamp(material.g, 0.0, 1.0);

    vec3 V = normalize(PC.cameraPosition.xyz - worldPosition);
    vec3 L = normalize(-PC.lightDirection.xyz);
    vec3 H = normalize(V + L);
    vec3 radiance = PC.lightColorIntensity.rgb * PC.lightColorIntensity.a;

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float VoH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(VoH, F0);

    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NoV * NoL, 0.0001);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * baseColor / PI;

    vec3 lighting = (diffuse + specular) * radiance * NoL;
    lighting += baseColor * PC.ambient.x;

    outColor = vec4(lighting, 1.0);
}
