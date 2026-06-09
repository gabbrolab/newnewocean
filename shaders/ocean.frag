#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;

uniform vec3 uBaseColor;
uniform float uAlpha;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(uLightDirection);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 80.0);
    float gridFade = smoothstep(70.0, 0.0, length(vWorldPosition.xz));
    vec3 color = uBaseColor * (0.28 + 0.72 * diffuse);
    color += vec3(0.62, 0.82, 0.90) * specular * 0.45;
    color = mix(color * 0.55, color, gridFade);
    FragColor = vec4(color, uAlpha);
}
