#pragma once

// Painted pigments and soft cel lighting, emitted in linear HDR.
inline constexpr const char* sceneFragmentShader = R"GLSL(
#version 330 core
in vec3 worldPos;
in vec3 worldNormal;
in vec3 vertexColor;
flat in int materialId;
in vec4 shadowPos;
layout(location = 0) out vec4 fragColor;
uniform vec3 cameraPos;
uniform float time;
uniform sampler2D shadowMap;
uniform sampler2D stonePaint;
uniform bool hasStonePaint;
uniform samplerCube pointShadow0;
uniform samplerCube pointShadow1;
uniform vec3 pointShadowPositions[2];
uniform float pointShadowFar;
uniform vec3 keyDirection;
uniform vec3 keyColor;
uniform int lightCount;
uniform vec3 lightPositions[16];
uniform vec3 lightColors[16];
uniform float lightRadii[16];

float hash(vec2 p)
{
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float paintNoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
vec2 surfaceCoordinates(vec3 p, vec3 n)
{
    vec3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}
float brushPaint(vec2 uv)
{
    // Broken pigment islands at a hand-sized scale, with short drybrush marks.
    vec2 p = mat2(0.94, 0.34, -0.34, 0.94) * uv;
    vec2 warp = vec2(paintNoise(p * 2.1 + 7.3), paintNoise(p * 2.3 - 4.1));
    p += (warp - 0.5) * 0.65;
    float underpaint = paintNoise(p * 3.2 + vec2(7.3, 2.1));
    float strokes = paintNoise(p * vec2(4.2, 7.0) + underpaint * 1.3);
    return mix(smoothstep(0.25, 0.76, underpaint),
               smoothstep(0.38, 0.67, strokes), 0.38);
}
const vec2 shadowDisk[16] = vec2[16](
    vec2(-0.613, 0.617), vec2(0.170, -0.040),
    vec2(-0.299, -0.791), vec2(0.645, 0.493),
    vec2(-0.652, -0.214), vec2(0.421, -0.697),
    vec2(0.006, 0.867), vec2(0.867, -0.133),
    vec2(-0.206, 0.326), vec2(-0.886, 0.148),
    vec2(0.371, 0.259), vec2(-0.043, -0.414),
    vec2(0.435, 0.841), vec2(-0.493, -0.489),
    vec2(0.789, -0.563), vec2(-0.298, 0.943));
float shadowVisibility(vec3 n, vec3 light)
{
    vec3 p = shadowPos.xyz / shadowPos.w;
    p = p * 0.5 + 0.5;
    if (p.z > 1.0 || p.z < 0.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
        return 1.0;
    float bias = max(0.00045 * (1.0 - dot(n, light)), 0.00013);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float blockerDepth = 0.0;
    float blockers = 0.0;
    for (int i = 0; i < 8; ++i) {
        float depth = texture(shadowMap, p.xy + shadowDisk[i] * texel * 7.0).r;
        if (depth < p.z - bias) {
            blockerDepth += depth;
            blockers += 1.0;
        }
    }
    if (blockers == 0.0) return 1.0;
    // Orthographic shadow camera: width 46, depth range 79.9 world units.
    float separation = max(p.z - blockerDepth / blockers, 0.0) * 79.9;
    float filterRadius = clamp(separation * 0.045 / 46.0 / texel.x, 1.0, 12.0);
    float visible = 0.0;
    for (int i = 0; i < 16; ++i)
        visible += p.z - bias <= texture(shadowMap, p.xy + shadowDisk[i] * texel * filterRadius).r ? 1.0 : 0.0;
    return visible / 16.0;
}
float torchVisibility(int shadowIndex, vec3 n)
{
    vec3 ray = worldPos - pointShadowPositions[shadowIndex];
    float distanceToLight = length(ray);
    if (pointShadowFar <= 0.0 || distanceToLight >= pointShadowFar || distanceToLight < 0.001)
        return 1.0;
    vec3 direction = ray / distanceToLight;
    vec3 helper = abs(direction.y) < 0.95 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(direction, helper));
    vec3 bitangent = cross(direction, tangent);
    float bias = mix(0.060, 0.035, max(dot(n, -direction), 0.0));
    float radius = mix(0.12, 0.32, clamp(distanceToLight / 12.0, 0.0, 1.0));
    float visible = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec3 sampleRay = ray + (tangent * shadowDisk[i].x + bitangent * shadowDisk[i].y) * radius;
        float depth;
        // GLSL 330 requires explicit sampler selection, not sampler-array indexing.
        if (shadowIndex == 0) depth = texture(pointShadow0, sampleRay).r;
        else depth = texture(pointShadow1, sampleRay).r;
        visible += distanceToLight - bias <= depth * pointShadowFar ? 1.0 : 0.0;
    }
    return visible / 16.0;
}
float celDiffuse(float cosine)
{
    // Three broad value bands with softened boundaries for stable motion.
    return 0.42 * smoothstep(0.10, 0.23, cosine)
         + 0.58 * smoothstep(0.57, 0.72, cosine);
}
vec3 paintedLight(vec3 albedo, vec3 n, vec3 view, vec3 light,
                  vec3 radiance, float metal, float paint)
{
    float nl = max(dot(n, light), 0.0);
    vec3 h = view + light;
    float hLength = length(h);
    float nh = hLength > 0.001 ? max(dot(n, h / hLength), 0.0) : 0.0;
    float highlight = smoothstep(0.81, 0.96, nh + (paint - 0.5) * 0.025)
                    * smoothstep(0.08, 0.22, nl);
    vec3 pigment = mix(vec3(0.15, 0.18, 0.21), albedo + vec3(0.10), 0.55);
    return radiance * (albedo * mix(celDiffuse(nl), nl, 0.60) * (1.0 - metal * 0.12)
                    + pigment * highlight * metal * 0.20);
}
void main()
{
    vec3 n = normalize(worldNormal);
    vec2 uv = surfaceCoordinates(worldPos, n);
    vec3 albedo = max(vertexColor, vec3(0.002));
    if (materialId == 7 || materialId == 8) {
        float pulse = 0.95 + 0.05 * sin(time * 1.7 + worldPos.y * 2.0);
        fragColor = vec4(albedo * (materialId == 7 ? 3.4 : 4.1) * pulse, 1.0);
        return;
    }
    float paint = brushPaint(uv);
    float broad = paintNoise(uv * 0.42 + vec2(12.4, 6.3));
    float metal = 0.0;
    float luminance = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
    if (materialId == 0 || materialId == 1) {
        vec3 slate = luminance * vec3(0.94, 0.97, 1.00);
        albedo = mix(albedo, slate, 0.80) * 0.67;
        albedo = mix(albedo * vec3(0.72, 0.73, 0.77),
                     albedo * vec3(1.10, 1.08, 1.02), paint);
        albedo *= 0.84 + broad * 0.23;
        float chipMask = smoothstep(0.55, 0.73, paintNoise(uv * 4.7 + 21.2));
        float drybrush = smoothstep(0.61, 0.79, paintNoise(uv * vec2(11.0, 17.0) + broad));
        albedo += luminance * vec3(0.15, 0.13, 0.095) * chipMask * drybrush;
        if (hasStonePaint) {
            // The painted bitmap stores pigment values, sampled as ordinary RGB.
            // A luminance ratio preserves authored material brightness and avoids
            // applying a second gamma conversion to the painted mid-gray.
            vec3 pigment = texture(stonePaint, uv * 0.22).rgb;
            float pigmentValue = max(dot(pigment, vec3(0.2126, 0.7152, 0.0722)), 0.02);
            float pigmentShade = clamp(pigmentValue / 0.45, 0.65, 1.25);
            vec3 pigmentTint = clamp(pigment / pigmentValue, vec3(0.75), vec3(1.25));
            albedo *= pigmentShade * mix(vec3(1.0), pigmentTint, 0.20);
        }
        float footWash = (1.0 - smoothstep(0.10, 1.5, worldPos.y)) * (1.0 - abs(n.y));
        albedo *= 1.0 - footWash * 0.17;
    } else if (materialId == 2) {
        float streak = smoothstep(0.23, 0.77,
                        paintNoise(uv * vec2(6.2, 0.65) + broad * 0.40));
        albedo = mix(albedo * vec3(0.77, 0.69, 0.79),
                     albedo * vec3(1.15, 1.06, 0.92), streak);
        albedo *= 0.91 + broad * 0.14;
    } else if (materialId == 3) {
        albedo = mix(albedo, luminance * vec3(0.78, 0.98, 1.18), 0.45);
        albedo *= mix(0.79, 1.13, paint);
        metal = 0.78;
    } else if (materialId == 4) {
        albedo = mix(albedo * vec3(0.79, 0.74, 0.86),
                     albedo * vec3(1.12, 1.04, 0.86), paint);
        metal = 0.84;
    } else if (materialId == 5 || materialId == 11) {
        albedo *= mix(0.82, 1.12, paint);
        albedo = mix(vec3(luminance), albedo, 1.12);
    } else if (materialId == 6) {
        albedo = mix(albedo * vec3(0.82, 0.78, 0.87),
                     albedo * vec3(1.03, 1.00, 0.92), paint);
    } else if (materialId == 9) {
        albedo = mix(albedo * vec3(0.72, 0.74, 0.93),
                     albedo * vec3(0.91, 1.03, 0.87), paint);
    } else {
        albedo *= 0.90 + paint * 0.17;
    }
    albedo = max(albedo, vec3(0.001));
    vec3 view = normalize(cameraPos - worldPos);
    vec3 key = normalize(keyDirection);
    float visibility = shadowVisibility(n, key);
    float hemisphere = n.y * 0.5 + 0.5;
    // Colored fill keeps the expanded room readable; occluded faces lean plum.
    vec3 plumAmbient = vec3(0.093, 0.071, 0.103) * 0.65;
    vec3 tealAmbient = vec3(0.122, 0.146, 0.157) * 0.65;
    float wash = (0.30 + hemisphere * 0.70) * (0.25 + visibility * 0.75);
    vec3 result = albedo * mix(plumAmbient, tealAmbient, wash);
    result += paintedLight(albedo, n, view, key, keyColor * visibility, metal, paint);
    for (int i = 0; i < 16; ++i) {
        if (i >= lightCount) break;
        vec3 delta = lightPositions[i] - worldPos;
        float distanceToLight = length(delta);
        float radius = max(lightRadii[i], 0.01);
        float falloff = max(1.0 - distanceToLight / radius, 0.0);
        falloff = falloff * falloff / (1.0 + distanceToLight * distanceToLight * 0.075);
        if (falloff > 0.0001) {
            float pointVisibility = 1.0;
            if (i == 2) pointVisibility = torchVisibility(0, n);
            else if (i == 3) pointVisibility = torchVisibility(1, n);
            result += paintedLight(albedo, n, view, delta / max(distanceToLight, 0.001),
                                    lightColors[i] * falloff * pointVisibility, metal, paint);
        }
    }
    float haze = min(0.10, (1.0 - exp(-length(cameraPos - worldPos) * 0.004)) * 0.5);
    result = mix(result, vec3(0.023, 0.027, 0.034), haze);
    fragColor = vec4(max(result, vec3(0.0)), 1.0);
}
)GLSL";
