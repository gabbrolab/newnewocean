#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform int uWaveMode;
uniform int uWaveCount;
uniform sampler2D uFftHeightMap;
uniform float uFftPatchLength;
uniform float uFftChoppiness;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec3 vSourcePosition;

const float PI = 3.14159265359;
const float GRAVITY = 9.81;
const int MAX_WAVES = 12;
const int GEOMETRY_WAVES = 7;

struct GerstnerWave {
    vec2 direction;
    float amplitude;
    float wavelength;
    float steepness;
    float phase;
};

uniform GerstnerWave uWaves[MAX_WAVES];

float fftMicroHeight(vec2 xz)
{
    vec2 d0 = normalize(vec2(0.92, 0.38));
    vec2 d1 = normalize(vec2(0.28, 0.96));
    vec2 d2 = normalize(vec2(-0.64, 0.77));
    float h = 0.0;
    h += sin(dot(xz, d0) * 0.72 + uTime * 1.35) * 0.090;
    h += sin(dot(xz, d1) * 1.18 + uTime * 1.92) * 0.050;
    h += sin(dot(xz, d2) * 1.82 + uTime * 2.35) * 0.028;
    return h;
}

vec2 coarseWarp(vec3 position)
{
    vec2 warp = vec2(0.0);

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount || i >= 3) {
            break;
        }

        GerstnerWave wave = uWaves[i];
        vec2 direction = normalize(wave.direction);
        float k = 2.0 * PI / wave.wavelength;
        float omega = sqrt(GRAVITY * k);
        float theta = k * dot(direction, position.xz) - omega * uTime + wave.phase;
        warp += direction * cos(theta) * wave.amplitude;
    }

    return warp * 0.16;
}

vec3 applyWaves(vec3 position)
{
    vec3 displaced = position;
    if (uWaveMode == 3) {
        vec2 uv = fract(position.xz / uFftPatchLength);
        displaced.y += texture(uFftHeightMap, uv).r + fftMicroHeight(position.xz);
        return displaced;
    }

    vec2 warp = coarseWarp(position);

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount || i >= GEOMETRY_WAVES) {
            break;
        }

        GerstnerWave wave = uWaves[i];
        vec2 direction = normalize(wave.direction);
        vec2 sampleXZ = position.xz;
        if (i >= 3) {
            sampleXZ += warp;
        }
        float k = 2.0 * PI / wave.wavelength;
        float omega = sqrt(GRAVITY * k);
        float theta = k * dot(direction, sampleXZ) - omega * uTime + wave.phase;

        if (uWaveMode == 1) {
            displaced.y += wave.amplitude * sin(theta);
        } else if (uWaveMode == 2) {
            displaced.xz += (wave.steepness / k) * direction * cos(theta);
            displaced.y += wave.amplitude * sin(theta);
        }
    }

    return displaced;
}

vec3 analyticalNormal(vec3 position)
{
    vec3 tangentX = vec3(1.0, 0.0, 0.0);
    vec3 tangentZ = vec3(0.0, 0.0, 1.0);
    vec2 warp = coarseWarp(position);

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount || i >= GEOMETRY_WAVES) {
            break;
        }

        GerstnerWave wave = uWaves[i];
        vec2 direction = normalize(wave.direction);
        vec2 sampleXZ = position.xz;
        if (i >= 3) {
            sampleXZ += warp;
        }
        float k = 2.0 * PI / wave.wavelength;
        float omega = sqrt(GRAVITY * k);
        float theta = k * dot(direction, sampleXZ) - omega * uTime + wave.phase;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        float q = wave.steepness;
        float a = wave.amplitude;

        if (uWaveMode == 1) {
            tangentX.y += a * k * direction.x * cosTheta;
            tangentZ.y += a * k * direction.y * cosTheta;
        } else if (uWaveMode == 2) {
            tangentX.x += -q * direction.x * direction.x * sinTheta;
            tangentX.y += a * k * direction.x * cosTheta;
            tangentX.z += -q * direction.x * direction.y * sinTheta;

            tangentZ.x += -q * direction.y * direction.x * sinTheta;
            tangentZ.y += a * k * direction.y * cosTheta;
            tangentZ.z += -q * direction.y * direction.y * sinTheta;
        }
    }

    return normalize(cross(tangentZ, tangentX));
}

void main()
{
    vec3 displaced = applyWaves(aPosition);
    vec4 worldPosition = uModel * vec4(displaced, 1.0);
    vWorldPosition = worldPosition.xyz;
    vSourcePosition = aPosition;
    vNormal = mat3(transpose(inverse(uModel))) * analyticalNormal(aPosition);
    gl_Position = uProjection * uView * worldPosition;
}
