#version 330 core
// Scene vertex stage: world transform, wind on banners and foliage, shadow coordinates.
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(location=3) in float material;
uniform mat4 viewProjection, model, lightMatrix;
uniform float time;
out vec3 worldPos, worldNormal, vertexColor;
flat out int materialId;
out vec4 shadowPos;
void main(){
    vec3 local = position;
    int id = int(material + 0.5);
    if (id == 11) {                       // banner cloth
        float weight = clamp((5.2 - local.y) / 2.8, 0.0, 1.0);
        local.z += weight * weight * (sin(time * 1.5 + local.y * 2.0 + local.x * 1.2) * 0.10 + sin(time * 2.3 + local.x * 3.0) * 0.035);
    }
    vec4 p = model * vec4(local, 1.0);
    if (id == 14) {                       // foliage sways with height above ground
        float h = max(p.y - 0.3, 0.0);
        p.x += sin(time * 1.3 + p.z * 0.35 + p.x * 0.2) * 0.035 * h;
        p.z += cos(time * 1.1 + p.x * 0.3) * 0.025 * h;
    }
    worldPos = p.xyz;
    worldNormal = mat3(model) * normal;
    vertexColor = color;
    materialId = id;
    shadowPos = lightMatrix * p;
    gl_Position = viewProjection * p;
}
