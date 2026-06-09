#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec3 vSourcePosition;

uniform vec3 uBaseColor;
uniform float uAlpha;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uFogColor;
uniform float uTime;
uniform int uWaveMode;
uniform int uWaveCount;

out vec4 FragColor;

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

float fresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
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

vec3 analyticalNormal(vec3 position)
{
    vec3 tangentX = vec3(1.0, 0.0, 0.0);
    vec3 tangentZ = vec3(0.0, 0.0, 1.0);
    vec2 warp = coarseWarp(position);

    for (int i = 0; i < MAX_WAVES; ++i) {
        if (i >= uWaveCount) {
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

vec3 skyEnvironment(vec3 direction)
{
    direction = normalize(direction);
    float horizon = smoothstep(-0.08, 0.22, direction.y);
    vec3 horizonColor = vec3(0.64, 0.48, 0.36);
    vec3 zenithColor = vec3(0.09, 0.18, 0.28);
    vec3 sky = mix(horizonColor, zenithColor, horizon);

    float sunDisk = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 650.0);
    float sunGlow = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 12.0);
    sky += vec3(1.0, 0.78, 0.42) * sunGlow * 0.25;
    sky += vec3(1.0, 0.92, 0.72) * sunDisk * 8.0;
    return sky;
}

vec3 acesToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(analyticalNormal(vSourcePosition));
    vec3 lightDirection = normalize(uLightDirection);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 140.0);
    float fresnel = fresnelSchlick(max(dot(normal, viewDirection), 0.0), 0.0204);
    vec3 reflectedDirection = reflect(-viewDirection, normal);
    vec3 reflection = skyEnvironment(reflectedDirection);
    float slope = clamp(1.0 - normal.y, 0.0, 1.0);
    float crestMask = smoothstep(0.35, 1.55, vWorldPosition.y) * smoothstep(0.04, 0.20, slope);
    float foamPattern = sin(vWorldPosition.x * 1.55 + vWorldPosition.z * 0.85 + vWorldPosition.y * 2.2);
    foamPattern += sin(vWorldPosition.x * -2.15 + vWorldPosition.z * 1.30);
    foamPattern += sin(vWorldPosition.x * 0.42 - vWorldPosition.z * 2.70);
    float foamBreakup = smoothstep(-0.25, 1.25, foamPattern);
    float foam = clamp(crestMask * (0.35 + 0.65 * foamBreakup), 0.0, 1.0);

    vec3 deepWater = vec3(0.010, 0.070, 0.095);
    vec3 shallowWater = vec3(0.035, 0.185, 0.225);
    vec3 waterColor = mix(deepWater, shallowWater, smoothstep(-1.4, 1.6, vWorldPosition.y));
    vec3 sunColor = vec3(1.00, 0.86, 0.62);
    vec3 glintColor = vec3(0.66, 0.88, 1.00);

    float gridFade = smoothstep(70.0, 0.0, length(vWorldPosition.xz));
    vec3 color = waterColor * (0.18 + 0.58 * diffuse);
    color = mix(color, reflection, clamp(fresnel * 1.15, 0.0, 0.62));
    color += sunColor * specular * (0.24 + 2.0 * fresnel);
    color += glintColor * fresnel * 0.08;
    color = mix(color, vec3(0.82, 0.96, 0.93), foam * 0.48);
    color = mix(color * 0.65, color, gridFade);

    float distanceToCamera = length(uCameraPosition - vWorldPosition);
    float heightFog = smoothstep(8.0, -2.0, vWorldPosition.y);
    float fog = 1.0 - exp(-distanceToCamera * 0.018);
    fog = clamp(fog * (0.35 + 0.65 * heightFog), 0.0, 0.82);
    color = mix(color, uFogColor, fog);
    color = acesToneMap(color * 0.82);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, uAlpha);
}
