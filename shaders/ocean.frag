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
const int DETAIL_NORMAL_START = 6;

const bool ENABLE_BASE_COLOR = true;
const bool ENABLE_DIFFUSE = true;
const bool ENABLE_SPECULAR = true;
const bool ENABLE_FRESNEL = true;
const bool ENABLE_REFLECTIONS = true;
const bool ENABLE_FOAM = true;
const bool ENABLE_NORMAL_DETAIL = true;

const float SPECULAR_STRENGTH = 0.26;
const float REFLECTION_STRENGTH = 0.64;
const float FOAM_STRENGTH = 1.85;
const float MICRO_NORMAL_STRENGTH = 0.78;

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

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbmNoise(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    mat2 rotate = mat2(0.80, -0.60, 0.60, 0.80);

    for (int i = 0; i < 4; ++i) {
        value += amplitude * valueNoise(p);
        p = rotate * p * 2.03 + vec2(17.1, 9.2);
        amplitude *= 0.5;
    }

    return value;
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
        if (i >= DETAIL_NORMAL_START) {
            float detailFade = 1.0 - smoothstep(16.0, 96.0, length(uCameraPosition.xz - position.xz));
            float detailStrength = ENABLE_NORMAL_DETAIL ? MICRO_NORMAL_STRENGTH : 0.0;
            q *= detailFade * detailStrength;
            a *= detailFade * detailStrength;
        }

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
    float distanceToCamera = length(uCameraPosition - vWorldPosition);

    if (uWaveMode == 3) {
        float height01 = smoothstep(-2.2, 2.2, vWorldPosition.y);
        float contour = smoothstep(0.045, 0.055, abs(fract(vWorldPosition.y * 1.6) - 0.5));
        vec3 trough = vec3(0.018, 0.085, 0.105);
        vec3 crest = vec3(0.42, 0.55, 0.52);
        vec3 color = mix(trough, crest, height01);
        color += vec3(0.08, 0.14, 0.13) * contour * 0.18;
        float fog = clamp(1.0 - exp(-distanceToCamera * 0.014), 0.0, 0.82);
        color = mix(color, uFogColor, fog);
        color = acesToneMap(color * 0.95);
        color = pow(color, vec3(1.0 / 2.2));
        FragColor = vec4(color, uAlpha);
        return;
    }

    float diffuse = ENABLE_DIFFUSE ? max(dot(normal, lightDirection), 0.0) : 0.0;
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 104.0);
    specular *= 1.0 - smoothstep(85.0, 260.0, distanceToCamera);
    specular *= ENABLE_SPECULAR ? SPECULAR_STRENGTH : 0.0;
    float fresnel = ENABLE_FRESNEL ? fresnelSchlick(max(dot(normal, viewDirection), 0.0), 0.0204) : 0.0204;
    vec3 reflectedDirection = reflect(-viewDirection, normal);
    vec3 reflection = ENABLE_REFLECTIONS ? skyEnvironment(reflectedDirection) : vec3(0.0);
    float slope = clamp(1.0 - normal.y, 0.0, 1.0);
    vec2 swellDirection = normalize(vec2(1.0, 0.15));
    vec2 crestDirection = vec2(-swellDirection.y, swellDirection.x);
    float crestMask = smoothstep(-0.22, 0.92, vWorldPosition.y) * smoothstep(0.015, 0.105, slope);
    float longFoam = sin(dot(vWorldPosition.xz, crestDirection) * 0.34 + dot(vWorldPosition.xz, swellDirection) * 0.055);
    float brokenFoam = smoothstep(-0.32, 0.58, longFoam + fbmNoise(vWorldPosition.xz * 0.075 + uTime * 0.025) * 0.55);
    float foamDistanceFade = 1.0 - smoothstep(90.0, 260.0, distanceToCamera);
    float foam = clamp(crestMask * brokenFoam * foamDistanceFade * FOAM_STRENGTH, 0.0, 1.0);
    foam *= ENABLE_FOAM ? 1.0 : 0.0;

    float height01 = smoothstep(-1.8, 1.6, vWorldPosition.y);
    float troughShade = smoothstep(-1.1, 0.55, vWorldPosition.y);
    float naturalVariation = fbmNoise(vSourcePosition.xz * 0.022 + vec2(uTime * 0.010, -uTime * 0.006));
    vec3 abyssWater = vec3(0.004, 0.038, 0.052);
    vec3 bodyWater = vec3(0.014, 0.085, 0.102);
    vec3 crestWater = vec3(0.045, 0.155, 0.160);
    vec3 waterColor = mix(abyssWater, bodyWater, troughShade);
    waterColor = mix(waterColor, crestWater, height01 * 0.42);
    waterColor *= 0.86 + naturalVariation * 0.20;
    if (!ENABLE_BASE_COLOR) {
        waterColor = vec3(0.0);
    }
    vec3 sunColor = vec3(1.00, 0.86, 0.62);
    vec3 glintColor = vec3(0.66, 0.88, 1.00);

    float gridFade = smoothstep(380.0, 40.0, length(vWorldPosition.xz));
    vec3 color = waterColor * (0.52 + 0.30 * diffuse);
    color += vec3(0.018, 0.055, 0.055) * slope * (0.35 + 0.65 * height01);
    float grazingReflection = smoothstep(0.018, 0.72, fresnel) * REFLECTION_STRENGTH;
    color = mix(color, reflection, clamp(grazingReflection, 0.0, 0.48));
    color += sunColor * specular * (0.14 + 1.65 * fresnel);
    color += glintColor * fresnel * 0.026;
    color = mix(color, vec3(0.90, 0.98, 0.95), foam * 0.95);
    color = mix(color * 0.65, color, gridFade);

    float heightFog = smoothstep(8.0, -2.0, vWorldPosition.y);
    float fog = 1.0 - exp(-distanceToCamera * 0.018);
    fog = clamp(fog * (0.40 + 0.60 * heightFog), 0.0, 0.86);
    color = mix(color, uFogColor, fog);
    color = acesToneMap(color * 0.82);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, uAlpha);
}
