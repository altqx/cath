#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D tex;

void main() {
  vec3 n = normalize(vNormal);
  // Stable lighting even with zero/missing normals from NIF extract
  if (length(n) < 1e-3) {
    n = vec3(0.0, 1.0, 0.0);
  }
  float ndl = clamp(dot(n, normalize(vec3(0.35, 0.85, 0.35))), 0.45, 1.0);
  vec4 albedo = texture(tex, vUv);

  // Degenerate UVs / black splash textures (e.g. atlus_logo) → lit debug skin
  float lum = dot(albedo.rgb, vec3(0.299, 0.587, 0.114));
  bool bad_sample = (albedo.a < 0.02) || (lum < 0.04);
  if (bad_sample) {
    // Hemisphere-ish tint so silhouettes read clearly on dark clear
    float hemi = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    albedo = vec4(mix(vec3(0.45, 0.28, 0.40), vec3(0.92, 0.72, 0.78), hemi), 1.0);
  }
  outColor = vec4(albedo.rgb * ndl, 1.0);
}
