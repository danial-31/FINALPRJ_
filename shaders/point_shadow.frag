#version 330 core
// Stores linear distance to the light, normalised by the far plane.
in vec3 lightWorldPos;
uniform vec3 lightPosition;
uniform float farPlane;
void main(){ gl_FragDepth = length(lightWorldPos - lightPosition) / farPlane; }
