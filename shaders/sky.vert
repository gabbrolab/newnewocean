#version 330 core

out vec3 vDirection;

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 clip = uv * 2.0 - 1.0;
    vec4 farPoint = inverse(uProjection * uView) * vec4(clip, 1.0, 1.0);
    vDirection = farPoint.xyz / farPoint.w;
    gl_Position = vec4(clip, 1.0, 1.0);
}

