#version 330 core
// Separable Gaussian blur (9 taps folded into 5 linear samples); the first pass extracts bright areas.
in vec2 uv; out vec4 fragColor;
uniform sampler2D source; uniform vec2 direction; uniform bool extract;
vec3 sampleColor(vec2 p){
    vec3 c = texture(source, p).rgb;
    return extract ? c * smoothstep(0.7, 1.8, max(c.r, max(c.g, c.b))) : c;
}
void main(){
    vec2 d = direction / vec2(textureSize(source, 0));
    vec3 c = sampleColor(uv) * 0.227027;
    c += (sampleColor(uv + d * 1.384615) + sampleColor(uv - d * 1.384615)) * 0.316216;
    c += (sampleColor(uv + d * 3.230769) + sampleColor(uv - d * 3.230769)) * 0.070270;
    fragColor = vec4(c, 1.0);
}
