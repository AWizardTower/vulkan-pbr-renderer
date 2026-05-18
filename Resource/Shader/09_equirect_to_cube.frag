#version 450

layout(set = 0, binding = 0) uniform sampler2D environmentHDR;

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

vec2 DirectionToEquirectUV(vec3 dir) {
    dir = normalize(dir);
    float phi = atan(dir.z, dir.x);
    float theta = asin(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, 0.5 - theta / PI);
}

void main() {
    uint face = uint(PC.params.x + 0.5);
    vec3 dir = CubeFaceDirection(face, fragUV);
    outColor = vec4(max(texture(environmentHDR, DirectionToEquirectUV(dir)).rgb, vec3(0.0)), 1.0);
}
