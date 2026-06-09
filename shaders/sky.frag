#version 330 core

in vec3 vDirection;

uniform vec3 uLightDirection;

out vec4 FragColor;

vec3 skyEnvironment(vec3 direction)
{
    direction = normalize(direction);
    float horizon = smoothstep(-0.10, 0.34, direction.y);
    vec3 horizonColor = vec3(0.96, 0.60, 0.34);
    vec3 zenithColor = vec3(0.025, 0.080, 0.150);
    vec3 sky = mix(horizonColor, zenithColor, horizon);

    float sunDisk = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 650.0);
    float sunGlow = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 10.0);
    sky += vec3(1.0, 0.72, 0.32) * sunGlow * 0.28;
    sky += vec3(1.0, 0.92, 0.72) * sunDisk * 8.0;
    return sky;
}

void main()
{
    FragColor = vec4(skyEnvironment(vDirection), 1.0);
}

