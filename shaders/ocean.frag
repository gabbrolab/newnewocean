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

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(uLightDirection);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 140.0);
    float fresnel = fresnelSchlick(max(dot(normal, viewDirection), 0.0), 0.0204);

    vec3 deepWater = vec3(0.015, 0.090, 0.115);
    vec3 shallowWater = vec3(0.050, 0.260, 0.300);
    vec3 waterColor = mix(deepWater, shallowWater, smoothstep(-1.4, 1.6, vWorldPosition.y));
    vec3 sunColor = vec3(1.00, 0.86, 0.62);
    vec3 glintColor = vec3(0.66, 0.88, 1.00);

    float gridFade = smoothstep(70.0, 0.0, length(vWorldPosition.xz));
    vec3 color = waterColor * (0.18 + 0.58 * diffuse);
    color += sunColor * specular * (0.35 + 2.4 * fresnel);
    color += glintColor * fresnel * 0.16;
    color = mix(color * 0.55, color, gridFade);
    FragColor = vec4(color, uAlpha);
}
