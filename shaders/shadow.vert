#version 330 core
// Depth-only pass for the sun shadow map and the omnidirectional torch maps.
layout(location=0) in vec3 position;
layout(location=3) in float material;
uniform mat4 lightMatrix, model;
uniform float time;
out vec3 lightWorldPos;
void main(){
    vec3 p = position;
    int id = int(material + 0.5);
    if (id == 11) { float w = clamp((5.2 - p.y) / 2.8, 0.0, 1.0); p.z += w * w * (sin(time * 1.5 + p.y * 2.0 + p.x * 1.2) * 0.10 + sin(time * 2.3 + p.x * 3.0) * 0.035); }
    vec4 world = model * vec4(p, 1.0);
    if (id == 14) { float h = max(world.y - 0.3, 0.0); world.x += sin(time * 1.3 + world.z * 0.35 + world.x * 0.2) * 0.035 * h; world.z += cos(time * 1.1 + world.x * 0.3) * 0.025 * h; }
    lightWorldPos = world.xyz;
    gl_Position = lightMatrix * world;
}
