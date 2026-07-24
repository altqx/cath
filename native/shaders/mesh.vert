#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(set = 0, binding = 0) uniform Frame {
  mat4 mvp;
  mat4 model;
} frame;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUv;

void main() {
  vNormal = mat3(frame.model) * inNormal;
  vUv = inUv;
  gl_Position = frame.mvp * vec4(inPos, 1.0);
}
