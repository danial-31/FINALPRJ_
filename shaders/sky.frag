#version 330 core
// Painted sky: vertical gradient, sun glow and drifting fbm clouds.
in vec2 uv; out vec4 fragColor;
uniform mat4 inverseViewProjection;
uniform vec3 sunDirection, skyTop, skyHorizon, cloudColor;
uniform float time, cloudAmount;
float hash(vec2 p){ vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }
float noise(vec2 p){ vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y); }
float fbm(vec2 p){ float v = 0.0, a = 0.5; for (int i = 0; i < 5; ++i) { v += a * noise(p); p = p * 2.03 + 7.1; a *= 0.5; } return v; }
void main(){
    vec4 far = inverseViewProjection * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 near = inverseViewProjection * vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - near.xyz / near.w);
    float h = clamp(dir.y * 1.6 + 0.25, 0.0, 1.0);
    vec3 sky = mix(skyHorizon, skyTop, pow(h, 0.8));
    float sun = max(dot(dir, normalize(sunDirection)), 0.0);
    sky += vec3(1.0, 0.75, 0.45) * (pow(sun, 350.0) * 6.0 + pow(sun, 12.0) * 0.25);
    if (dir.y > -0.05 && cloudAmount > 0.0) {
        vec2 cp = dir.xz / (dir.y + 0.18) * 1.2 + vec2(time * 0.012, time * 0.004);
        float c = smoothstep(0.48, 0.78, fbm(cp));
        float shade = fbm(cp * 1.7 + 3.0);
        vec3 cloud = mix(cloudColor * 0.75, cloudColor * 1.15, shade);
        sky = mix(sky, cloud, c * cloudAmount * smoothstep(-0.05, 0.15, dir.y));
    }
    fragColor = vec4(sky, 1.0);
}
