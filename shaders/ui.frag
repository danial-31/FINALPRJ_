#version 330 core
// Glyph atlas alpha for text; a negative u coordinate marks a solid quad.
in vec2 uv; in vec4 tint; out vec4 fragColor; uniform sampler2D atlas;
flat in float painted;
uniform sampler2D icons;
uniform sampler2D emblems;
uniform sampler2D cover;
void main(){
    if(painted > 2.5) fragColor = texture(cover,uv) * tint;
    else if(painted > 1.5) fragColor = texture(emblems,uv) * tint;
    else if(painted > .5) fragColor = texture(icons,uv) * tint;
    else fragColor = vec4(tint.rgb, tint.a * (uv.x < 0.0 ? 1.0 : texture(atlas, uv).a));
}
