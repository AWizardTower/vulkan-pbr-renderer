#version 450

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_Texcoord;
layout(location = 2) in vec3 a_Normal;

layout(push_constant) uniform PushConstants{
    mat4 mvp;
    vec4 worldRows[3];
    vec4 material;
} PC;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) out vec3 fragWorldPosition;

vec3 TransformWorldPosition(vec3 localPosition) {
    vec4 local = vec4(localPosition, 1.0);
    return vec3(
        dot(PC.worldRows[0], local),
        dot(PC.worldRows[1], local),
        dot(PC.worldRows[2], local)
    );
}

vec3 TransformWorldNormal(vec3 localNormal) {
    return normalize(vec3(
        dot(PC.worldRows[0].xyz, localNormal),
        dot(PC.worldRows[1].xyz, localNormal),
        dot(PC.worldRows[2].xyz, localNormal)
    ));
}

void main() {
    fragUV = a_Texcoord;
    fragWorldPosition = TransformWorldPosition(a_Pos);
    fragWorldNormal = TransformWorldNormal(a_Normal);
    gl_Position = PC.mvp * vec4(a_Pos, 1.0);
}
