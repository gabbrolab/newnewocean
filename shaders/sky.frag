#version 330 core

in vec3 vDirection;

uniform vec3 uLightDirection;

out vec4 FragColor;

vec3 skyEnvironment(vec3 direction)
{
    direction = normalize(direction);
    float horizon = smoothstep(-0.10, 0.34, direction.y);
    vec3 horizonColor = vec3(0.66, 0.48, 0.35);
    vec3 zenithColor = vec3(0.10, 0.20, 0.32);
    vec3 sky = mix(horizonColor, zenithColor, horizon);

    float sunDisk = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 650.0);
    float sunGlow = pow(max(dot(direction, normalize(uLightDirection)), 0.0), 10.0);
    sky += vec3(1.0, 0.72, 0.32) * sunGlow * 0.18;
    sky += vec3(1.0, 0.92, 0.72) * sunDisk * 4.0;
    return sky;
}

void main()
{
    vec3 color = skyEnvironment(vDirection);
    color = clamp(color / (color + vec3(1.0)), 0.0, 1.0);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
