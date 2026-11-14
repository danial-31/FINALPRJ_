#version 330 core
// Point sprites; size is attenuated by clip w under perspective projection.
layout(location=0) in vec3 position; layout(location=1) in vec3 color;
layout(location=2) in float size; layout(location=3) in float alpha;
layout(location=4) in float kind;
uniform mat4 viewProjection; uniform float pointScale; uniform bool perspectiveCamera;
out vec3 particleColor; out float opacity; flat out int type;
void main(){
    gl_Position = viewProjection * vec4(position, 1.0);
    float w = perspectiveCamera ? gl_Position.w : 1.0;
    gl_PointSize = clamp(size * pointScale / w, 1.0, 96.0);
    particleColor = color; opacity = alpha; type = int(kind);
}
