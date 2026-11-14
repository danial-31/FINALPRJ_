#version 330 core
// Composite: depth-aware ambient darkening, ink outlines, bloom, ACES tone mapping, vignette.
in vec2 uv; out vec4 fragColor;
uniform sampler2D scene, bloom, depth;
uniform float time, nearPlane, farPlane, bloomStrength, fade, exposure, saturation;
uniform bool perspectiveCamera, effects, tiltShift;
float linearDepth(float z){
    if (!perspectiveCamera) return z;
    float ndc = z * 2.0 - 1.0;
    float view = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - ndc * (farPlane - nearPlane));
    return (view - nearPlane) / (farPlane - nearPlane);
}
vec3 aces(vec3 x){ return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main(){
    vec3 c = texture(scene, uv).rgb;
    if (tiltShift) {
        // Miniature look: a sharp horizontal band, blur growing towards the top and bottom edges.
        float blur = smoothstep(0.16, 0.48, abs(uv.y - 0.52)) * 5.0;
        if (blur > 0.05) {
            vec2 texel = 1.0 / vec2(textureSize(scene, 0));
            vec3 sum = c; float weight = 1.0;
            for (int i = 0; i < 12; ++i) {
                float a = float(i) * 2.39996;               // golden-angle spiral
                float r = sqrt(float(i + 1) / 12.0) * blur;
                sum += texture(scene, uv + vec2(cos(a), sin(a)) * r * texel * 1.6).rgb;
                weight += 1.0;
            }
            c = sum / weight;
        }
    }
    float raw = texture(depth, uv).r;
    if (raw < 0.9999 && effects) {
        float z = linearDepth(raw);
        vec2 texel = 1.0 / vec2(textureSize(depth, 0));
        float ao = 0.0;
        for (int i = 0; i < 8; ++i) {
            float a = float(i) * 0.785398;
            float dz = (z - linearDepth(texture(depth, uv + vec2(cos(a), sin(a)) * texel * 5.0).r)) * 65.0;
            ao += smoothstep(0.025, 0.20, dz) * (1.0 - smoothstep(0.35, 1.4, dz));
        }
        c *= 1.0 - ao * 0.025;
        float l = linearDepth(texture(depth, uv - vec2(texel.x, 0)).r);
        float r = linearDepth(texture(depth, uv + vec2(texel.x, 0)).r);
        float u = linearDepth(texture(depth, uv - vec2(0, texel.y)).r);
        float d = linearDepth(texture(depth, uv + vec2(0, texel.y)).r);
        // Second difference of depth, relative to distance, marks silhouettes and creases.
        float crease = max(abs(l + r - 2.0 * z), abs(u + d - 2.0 * z)) / max(z * 6.0, 0.05);
        float ink = smoothstep(0.00035, 0.0018, crease);
        c = mix(c, c * 0.28 + vec3(0.004, 0.005, 0.012), ink * 0.6);
    }
    c += texture(bloom, uv).rgb * bloomStrength;
    c = aces(c * exposure);
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = clamp(mix(vec3(luma), c, saturation), 0.0, 1.0);   // painterly colour: restore saturation lost in tone mapping
    float vignette = 1.0 - 0.32 * pow(length((uv - 0.5) * vec2(1.15, 1.0)), 1.5);
    c *= vignette;
    c = pow(c, vec3(1.0 / 2.2));
    float grain = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    fragColor = vec4((c + grain * 0.0022) * fade, 1.0);
}
