#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec3 vSourcePosition;

uniform vec3 uBaseColor;
uniform float uAlpha;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
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
    vec3 horizonColor = vec3(0.92, 0.62, 0.36);
    vec3 zenithColor = vec3(0.05, 0.16, 0.28);
    vec3 sky = mix(horizonColor, zenithColor, horizon);

    float sunDisk = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 650.0);
    float sunGlow = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 12.0);
    sky += vec3(1.0, 0.78, 0.42) * sunGlow * 0.25;
    sky += vec3(1.0, 0.92, 0.72) * sunDisk * 8.0;
    return sky;
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

    vec3 deepWater = vec3(0.015, 0.090, 0.115);
    vec3 shallowWater = vec3(0.050, 0.260, 0.300);
    vec3 waterColor = mix(deepWater, shallowWater, smoothstep(-1.4, 1.6, vWorldPosition.y));
    vec3 sunColor = vec3(1.00, 0.86, 0.62);
    vec3 glintColor = vec3(0.66, 0.88, 1.00);

    float gridFade = smoothstep(70.0, 0.0, length(vWorldPosition.xz));
    vec3 color = waterColor * (0.18 + 0.58 * diffuse);
    color = mix(color, reflection, clamp(fresnel * 1.8, 0.0, 0.85));
    color += sunColor * specular * (0.35 + 2.4 * fresnel);
    color += glintColor * fresnel * 0.16;
    color = mix(color, vec3(0.82, 0.96, 0.93), foam * 0.78);
    color = mix(color * 0.55, color, gridFade);
    FragColor = vec4(color, uAlpha);
}
