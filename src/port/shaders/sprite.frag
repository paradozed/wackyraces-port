#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(push_constant) uniform Transform { mat4 mvp; vec4 fog_color; vec4 fog_range; } transform;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=2) in float fog;
layout(location=0) out vec4 color;
void main() {
    color = texture(image, uv) * tint;
    if (color.a == 0.0) discard;
    color.rgb = mix(color.rgb, transform.fog_color.rgb, fog);
}
