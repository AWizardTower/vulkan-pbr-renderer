#version 450

layout(set = 0, binding = 0) uniform samplerCube environmentCube;

layout(push_constant) uniform IBLPrecomputePushConstants{
    vec4 params;
} PC;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 CubeFaceDirection(uint face, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    if(face == 0u) { return normalize(vec3( 1.0, -p.y, -p.x)); }
    if(face == 1u) { return normalize(vec3(-1.0, -p.y,  p.x)); }
    if(face == 2u) { return normalize(vec3( p.x,  1.0,  p.y)); }
    if(face == 3u) { return normalize(vec3( p.x, -1.0, -p.y)); }
    if(face == 4u) { return normalize(vec3( p.x, -p.y,  1.0)); }
    return normalize(vec3(-p.x, -p.y, -1.0));
}

float RadicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    uint face = uint(PC.params.x + 0.5);
    float roughness = clamp(PC.params.y, 0.0, 1.0);
    float sourceMaxMip = max(PC.params.z, 0.0);
    vec3 N = CubeFaceDirection(face, fragUV);
    vec3 R = N;
    vec3 V = R;

    const uint sampleCount = 64u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    float sourceDim = float(textureSize(environmentCube, 0).x);

    for(uint i = 0u; i < sampleCount; ++i) {
        vec2 Xi = Hammersley(i, sampleCount);
        vec3 H = ImportanceSampleGGX(Xi, max(roughness, 0.04), N);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NoL = max(dot(N, L), 0.0);
        if(NoL > 0.0) {
            float NoH = max(dot(N, H), 0.0);
            float HoV = max(dot(H, V), 0.0);
            float a = roughness * roughness;
            float a2 = a * a;
            float denom = NoH * NoH * (a2 - 1.0) + 1.0;
            float D = a2 / max(PI * denom * denom, 0.0001);
            float pdf = max(D * NoH / (4.0 * HoV), 0.0001);
            float saTexel = 4.0 * PI / (6.0 * sourceDim * sourceDim);
            float saSample = 1.0 / (float(sampleCount) * pdf);
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
            prefilteredColor += textureLod(environmentCube, L, clamp(mipLevel, 0.0, sourceMaxMip)).rgb * NoL;
            totalWeight += NoL;
        }
    }

    prefilteredColor /= max(totalWeight, 0.0001);
    outColor = vec4(max(prefilteredColor, vec3(0.0)), 1.0);
}
