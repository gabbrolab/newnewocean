#version 430 core

in vec3 vWorldPosition;
in vec2 vWorldUV;

out vec4 FragColor;

uniform sampler2DArray uDisplacement;
uniform sampler2DArray uSlope;
uniform sampler2DArray uInitialSpectrum;
uniform int uCascadeCount;
uniform int uDebugView;
uniform int uDebugCascade;
uniform float uLengthScales[4];
uniform float uTiles[4];

uniform vec3 uCameraPosition;
uniform vec3 uSunDirection;   // direction towards the sun
uniform vec3 uSunColor;       // sun irradiance
uniform vec3 uFogColor;

uniform float uNormalStrength;
uniform float uRoughness;
uniform float uFoamRoughnessModifier;
uniform float uHeightModifier;

uniform vec3 uScatterColor;
uniform vec3 uBubbleColor;
uniform vec3 uFoamColor;
uniform float uBubbleDensity;
uniform float uWavePeakScatterStrength;
uniform float uScatterStrength;
uniform float uScatterShadowStrength;
uniform float uEnvironmentLightStrength;

const float PI = 3.14159265358979323846;

bool cascadeEnabled(int cascade)
{
    return uDebugCascade < 0 || cascade == uDebugCascade;
}

int debugCascadeIndex()
{
    return clamp(uDebugCascade < 0 ? 0 : uDebugCascade, 0, uCascadeCount - 1);
}

vec3 cascadeTint(int cascade)
{
    if (cascade == 0) {
        return vec3(0.20, 0.55, 1.00);
    }
    if (cascade == 1) {
        return vec3(0.05, 0.95, 0.70);
    }
    if (cascade == 2) {
        return vec3(1.00, 0.72, 0.20);
    }
    return vec3(1.00, 0.28, 0.44);
}

vec3 skyEnvironment(vec3 direction)
{
    direction = normalize(direction);
    float horizon = smoothstep(-0.08, 0.22, direction.y);
    vec3 horizonColor = vec3(0.64, 0.48, 0.36);
    vec3 zenithColor = vec3(0.09, 0.18, 0.28);
    vec3 sky = mix(horizonColor, zenithColor, horizon);

    vec3 sunDir = normalize(uSunDirection);
    float sunGlow = pow(max(dot(direction, sunDir), 0.0), 12.0);
    float sunDisk = pow(max(dot(direction, sunDir), 0.0), 650.0);
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

float smithMaskingBeckmann(vec3 h, vec3 s, float roughness)
{
    float hdots = max(0.001, clamp(dot(h, s), 0.0, 1.0));
    float a = hdots / (roughness * sqrt(1.0 - hdots * hdots));
    float a2 = a * a;
    return a < 1.6 ? (1.0 - 1.259 * a + 0.396 * a2) / (3.535 * a + 2.181 * a2) : 0.0;
}

float beckmann(float ndoth, float roughness)
{
    float expArg = (ndoth * ndoth - 1.0) / (roughness * roughness * ndoth * ndoth);
    return exp(expArg) / (PI * roughness * roughness * ndoth * ndoth * ndoth * ndoth);
}

void main()
{
    vec4 displacementFoam = vec4(0.0);
    vec2 slopes = vec2(0.0);
    for (int c = 0; c < uCascadeCount; ++c) {
        if (!cascadeEnabled(c)) {
            continue;
        }
        vec2 uv = vWorldUV / uLengthScales[c] * uTiles[c];
        displacementFoam += texture(uDisplacement, vec3(uv, float(c)));
        slopes += texture(uSlope, vec3(uv, float(c))).xy;
    }

    slopes *= uNormalStrength;

    float distanceToCamera = length(uCameraPosition - vWorldPosition);
    // Gently soften normals and foam with distance to curb specular sparkle and
    // foam shimmer, but keep most of the detail so the far field stays alive
    // (the slope mipmaps already do most of the anti-aliasing).
    float detailFade = 1.0 - smoothstep(180.0, 700.0, distanceToCamera);

    float foam = clamp(displacementFoam.a, 0.0, 1.0) * mix(0.6, 1.0, detailFade);

    vec3 normal = normalize(vec3(-slopes.x, 1.0, -slopes.y));
    normal = normalize(mix(vec3(0.0, 1.0, 0.0), normal, mix(0.6, 1.0, detailFade)));

    if (uDebugView == 1) {
        float height = clamp(displacementFoam.y * 0.06 + 0.5, 0.0, 1.0);
        FragColor = vec4(vec3(height), 1.0);
        return;
    }
    if (uDebugView == 2) {
        float slope = clamp(length(slopes) * 0.08, 0.0, 1.0);
        FragColor = vec4(mix(vec3(0.02, 0.07, 0.10), vec3(0.82, 0.95, 1.00), slope), 1.0);
        return;
    }
    if (uDebugView == 3) {
        float crestPotential = clamp(length(slopes) * 0.08 + max(displacementFoam.y, 0.0) * 0.05, 0.0, 1.0);
        float foamDebug = clamp(max(foam * 4.0, crestPotential * 0.65), 0.0, 1.0);
        FragColor = vec4(mix(vec3(0.00, 0.04, 0.06), uFoamColor, foamDebug), 1.0);
        return;
    }
    if (uDebugView == 4) {
        int cascade = debugCascadeIndex();
        float signal = clamp(abs(displacementFoam.y) * 0.22 + length(slopes) * 0.08 + foam * 2.0, 0.0, 1.0);
        FragColor = vec4(cascadeTint(cascade) * (0.28 + 0.72 * signal), 1.0);
        return;
    }
    if (uDebugView == 5) {
        int cascade = debugCascadeIndex();
        vec2 uv = fract(gl_FragCoord.xy / vec2(512.0));
        vec4 spectrum = texture(uInitialSpectrum, vec3(uv, float(cascade)));
        float energy = clamp(log(1.0 + length(spectrum) * 60000.0) * 0.22, 0.0, 1.0);
        FragColor = vec4(mix(vec3(0.01, 0.02, 0.05), cascadeTint(cascade), energy), 1.0);
        return;
    }
    if (uDebugView == 6) {
        FragColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }

    vec3 lightDir = normalize(uSunDirection);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    vec3 macroNormal = vec3(0.0, 1.0, 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Cook-Torrance style specular with Beckmann distribution and Smith masking.
    float a = uRoughness + foam * uFoamRoughnessModifier;
    float ndoth = max(0.0001, dot(normal, halfwayDir));
    float viewMask = smithMaskingBeckmann(halfwayDir, viewDir, a);
    float lightMask = smithMaskingBeckmann(halfwayDir, lightDir, a);
    float g = 1.0 / (1.0 + viewMask + lightMask);

    float eta = 1.33;
    float r0 = ((eta - 1.0) * (eta - 1.0)) / ((eta + 1.0) * (eta + 1.0));
    float numerator = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0 * exp(-2.69 * a));
    float fresnel = r0 + (1.0 - r0) * numerator / (1.0 + 22.7 * pow(a, 1.5));
    fresnel = clamp(fresnel, 0.0, 1.0);

    vec3 specular = uSunColor * fresnel * g * beckmann(ndoth, a);
    specular /= 4.0 * max(0.001, dot(macroNormal, lightDir));
    specular *= max(dot(normal, lightDir), 0.0);

    vec3 envReflection = skyEnvironment(reflect(-viewDir, normal)) * uEnvironmentLightStrength;

    // Subsurface / scattering approximation.
    float waveHeight = max(0.0, displacementFoam.y) * uHeightModifier;
    float k1 = uWavePeakScatterStrength * waveHeight
        * pow(max(dot(lightDir, -viewDir), 0.0), 4.0)
        * pow(0.5 - 0.5 * dot(lightDir, normal), 3.0);
    float k2 = uScatterStrength * pow(max(dot(viewDir, normal), 0.0), 2.0);
    float k3 = uScatterShadowStrength * NdotL;
    float k4 = uBubbleDensity;

    vec3 scatter = (k1 + k2) * uScatterColor * uSunColor / (1.0 + lightMask);
    scatter += k3 * uScatterColor * uSunColor + k4 * uBubbleColor * uSunColor;

    vec3 color = (1.0 - fresnel) * scatter + specular + fresnel * envReflection;
    color = max(vec3(0.0), color);
    color = mix(color, uFoamColor, foam);

    // Distance fog and tone mapping. Matches the gerstner branch's atmosphere
    // (dark blue haze, height-weighted) so both scenes share the same backdrop.
    float heightFog = smoothstep(8.0, -2.0, vWorldPosition.y);
    float fog = 1.0 - exp(-distanceToCamera * 0.006);
    fog = clamp(fog * (0.40 + 0.60 * heightFog), 0.0, 0.86);
    color = mix(color, uFogColor, fog);

    color = acesToneMap(color);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
