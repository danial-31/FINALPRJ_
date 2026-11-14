#version 330 core
// Glyph atlas alpha for text; a negative u coordinate marks a solid quad.
in vec2 uv; in vec4 tint; out vec4 fragColor; uniform sampler2D atlas;
void main(){ fragColor = vec4(tint.rgb, tint.a * (uv.x < 0.0 ? 1.0 : texture(atlas, uv).a)); }
