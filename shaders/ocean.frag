#version 330 core

in vec3 vWorldPosition;

uniform vec3 uBaseColor;
uniform float uAlpha;

out vec4 FragColor;

void main()
{
    float gridFade = smoothstep(70.0, 0.0, length(vWorldPosition.xz));
    vec3 color = mix(uBaseColor * 0.55, uBaseColor, gridFade);
    FragColor = vec4(color, uAlpha);
}

