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

float sineHeight(vec3 position, vec2 direction, float amplitude, float wavelength, float phase)
{
    float k = 2.0 * PI / wavelength;
    float omega = sqrt(GRAVITY * k);
    float theta = k * dot(normalize(direction), position.xz) - omega * uTime + phase;
    return amplitude * sin(theta);
}

vec3 applySineWave(vec3 position)
{
    if (uWaveMode == 0) {
        return position;
    }

    position.y += sineHeight(position, vec2(1.0, 0.22), 2.1, 24.0, 0.0);
    if (uWaveMode >= 2) {
        position.y += sineHeight(position, vec2(0.35, 0.94), 0.92, 17.0, 1.7);
        position.y += sineHeight(position, vec2(-0.62, 0.78), 0.54, 11.0, 3.2);
        position.y += sineHeight(position, vec2(0.88, -0.48), 0.34, 7.5, 5.1);
    }
    return position;
}

void main()
{
    vec3 displaced = applySineWave(aPosition);
    vec4 worldPosition = uModel * vec4(displaced, 1.0);
    vWorldPosition = worldPosition.xyz;
    gl_Position = uProjection * uView * worldPosition;
}

