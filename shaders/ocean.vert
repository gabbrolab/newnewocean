#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform int uWaveMode;

out vec3 vWorldPosition;

const float PI = 3.14159265359;
const float GRAVITY = 9.81;

vec3 applySineWave(vec3 position)
{
    if (uWaveMode == 0) {
        return position;
    }

    vec2 direction = normalize(vec2(1.0, 0.22));
    float amplitude = 2.1;
    float wavelength = 24.0;
    float k = 2.0 * PI / wavelength;
    float omega = sqrt(GRAVITY * k);
    float theta = k * dot(direction, position.xz) - omega * uTime;

    position.y += amplitude * sin(theta);
    return position;
}

void main()
{
    vec3 displaced = applySineWave(aPosition);
    vec4 worldPosition = uModel * vec4(displaced, 1.0);
    vWorldPosition = worldPosition.xyz;
    gl_Position = uProjection * uView * worldPosition;
}

