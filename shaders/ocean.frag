#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;

uniform vec3 uBaseColor;
uniform float uAlpha;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;

out vec4 FragColor;

float fresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
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
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(uLightDirection);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 140.0);
    float fresnel = fresnelSchlick(max(dot(normal, viewDirection), 0.0), 0.0204);
    vec3 reflectedDirection = reflect(-viewDirection, normal);
    vec3 reflection = skyEnvironment(reflectedDirection);

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
    color = mix(color * 0.55, color, gridFade);
    FragColor = vec4(color, uAlpha);
}
