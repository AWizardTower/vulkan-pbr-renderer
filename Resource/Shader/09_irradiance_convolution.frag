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

void main() {
    uint face = uint(PC.params.x + 0.5);
    vec3 N = CubeFaceDirection(face, fragUV);
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);
    float sampleCount = 0.0;
    const float sampleDelta = 0.18;
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVector = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            irradiance += textureLod(environmentCube, sampleVector, 0.0).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }

    irradiance = PI * irradiance / max(sampleCount, 1.0);
    outColor = vec4(max(irradiance, vec3(0.0)), 1.0);
}
