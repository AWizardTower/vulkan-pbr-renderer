#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

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

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float GeometrySchlickGGX(float NoV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NoV / max(NoV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(float NoV, float NoL, float roughness) {
    return GeometrySchlickGGX(NoV, roughness) * GeometrySchlickGGX(NoL, roughness);
}

vec2 IntegrateBRDF(float NoV, float roughness) {
    vec3 V = vec3(sqrt(max(1.0 - NoV * NoV, 0.0)), 0.0, NoV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;
    const uint sampleCount = 128u;
    for(uint i = 0u; i < sampleCount; ++i) {
        vec2 Xi = Hammersley(i, sampleCount);
        vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NoL = max(L.z, 0.0);
        float NoH = max(H.z, 0.0);
        float VoH = max(dot(V, H), 0.0);
        if(NoL > 0.0) {
            float G = GeometrySmith(NoV, NoL, roughness);
            float GVis = (G * VoH) / max(NoH * NoV, 0.0001);
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * GVis;
            B += Fc * GVis;
        }
    }
    return vec2(A, B) / float(sampleCount);
}

void main() {
    vec2 brdf = IntegrateBRDF(clamp(fragUV.x, 0.001, 1.0), clamp(fragUV.y, 0.001, 1.0));
    outColor = vec4(brdf, 0.0, 1.0);
}
