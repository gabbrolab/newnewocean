#version 430 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

uniform sampler2DArray uDisplacement;
uniform int uCascadeCount;
uniform int uDebugCascade;
uniform float uLengthScales[4];
uniform float uTiles[4];
uniform vec3 uCameraPosition;

out vec3 vWorldPosition;
out vec2 vWorldUV;

bool cascadeEnabled(int cascade)
{
    return uDebugCascade < 0 || cascade == uDebugCascade;
}

void main()
{
    // The model matrix is identity, so object space xz equals world space xz.
    vec3 world = (uModel * vec4(aPosition, 1.0)).xyz;

    vec3 displacement = vec3(0.0);
    for (int c = 0; c < uCascadeCount; ++c) {
        if (!cascadeEnabled(c)) {
            continue;
        }
        vec2 uv = world.xz / uLengthScales[c] * uTiles[c];
        displacement += textureLod(uDisplacement, vec3(uv, float(c)), 0.0).xyz;
    }

    // Distance handling: the jagged "big waves" at the horizon come from the
    // horizontal choppiness (x/z displacement) seen at grazing angles, so fade
    // that out with distance. Keep the vertical swell (only gently reduced) so
    // the ocean still rolls to the horizon instead of going dead flat.
    float distanceToCamera = length(uCameraPosition - world);
    float choppyFade = 1.0 - smoothstep(120.0, 450.0, distanceToCamera);
    float heightFade = mix(1.0, 0.55, smoothstep(150.0, 700.0, distanceToCamera));
    displacement.xz *= choppyFade;
    displacement.y *= heightFade;

    vec3 displaced = aPosition + displacement;
    vec4 worldDisplaced = uModel * vec4(displaced, 1.0);

    vWorldPosition = worldDisplaced.xyz;
    // Sample the slope/foam maps from the undisplaced domain to avoid swimming.
    vWorldUV = world.xz;

    gl_Position = uProjection * uView * worldDisplaced;
}
