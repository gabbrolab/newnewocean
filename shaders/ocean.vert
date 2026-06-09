#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform int uWaveMode;
uniform int uWaveCount;

out vec3 vWorldPosition;

const float PI = 3.14159265359;
const float GRAVITY = 9.81;
const int MAX_WAVES = 12;

struct GerstnerWave {
    vec2 direction;
    float amplitude;
    float wavelength;
    float steepness;
    float phase;
};

uniform GerstnerWave uWaves[MAX_WAVES];

vec3 applyWaves(vec3 position)
{
    vec3 displaced = position;

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount) {
            break;
        }

        GerstnerWave wave = uWaves[i];
        vec2 direction = normalize(wave.direction);
        float k = 2.0 * PI / wave.wavelength;
        float omega = sqrt(GRAVITY * k);
        float theta = k * dot(direction, position.xz) - omega * uTime + wave.phase;

        if (uWaveMode == 1) {
            displaced.y += wave.amplitude * sin(theta);
        } else if (uWaveMode == 2) {
            displaced.xz += (wave.steepness / k) * direction * cos(theta);
            displaced.y += wave.amplitude * sin(theta);
        }
    }

    return displaced;
}

void main()
{
    vec3 displaced = applyWaves(aPosition);
    vec4 worldPosition = uModel * vec4(displaced, 1.0);
    vWorldPosition = worldPosition.xyz;
    gl_Position = uProjection * uView * worldPosition;
}
