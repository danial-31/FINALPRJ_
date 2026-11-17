#version 330 core
// Screen-space HUD quads given in pixels.
layout(location=0) in vec2 p; layout(location=1) in vec2 tex; layout(location=2) in vec4 color;
uniform vec2 viewport; out vec2 uv; out vec4 tint;
layout(location=3) in float useImage;
flat out float painted;
void main(){ gl_Position = vec4(p.x / viewport.x * 2.0 - 1.0, 1.0 - p.y / viewport.y * 2.0, 0.0, 1.0); uv = tex; tint = color; painted=useImage; }
