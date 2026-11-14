#version 330 core
// Soft round sprite; smoke (type 2) uses a wider falloff.
in vec3 particleColor; in float opacity; flat in int type; out vec4 fragColor;
void main(){
    vec2 p = gl_PointCoord * 2.0 - 1.0; float r = length(p);
    if (r > 1.0) discard;
    float a = pow(max(0.0, 1.0 - r * r), type == 2 ? 3.0 : 1.8) * opacity;
    fragColor = vec4(particleColor, a);
}
