#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec4 color;
// fog_range.xy: linear fog start/end in view depth (clip w); end<=start disables.
layout(push_constant) uniform Transform { mat4 mvp; vec4 fog_color; vec4 fog_range; } transform;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
layout(location=2) out float fog;
void main() {
    gl_Position = transform.mvp * vec4(position, 1.0);
    uv = texcoord;
    tint = color;
    float span = transform.fog_range.y - transform.fog_range.x;
    fog = span > 0.0 ? clamp((gl_Position.w - transform.fog_range.x) / span, 0.0, 1.0) : 0.0;
}
