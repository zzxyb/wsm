#version 450
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 48) vec2 halfpixel;
	float offset;
	int capture_encoding;
} data;
#define halfpixel data.halfpixel
#define offset data.offset
vec4 sample_blur(vec2 point) {
	vec4 color = texture(tex, point);
	if (data.capture_encoding == 0) {
		return color;
	}
	vec3 rgb = color.a > 0.0 ? max(color.rgb / color.a, vec3(0)) : vec3(0);
	if (data.capture_encoding == 2) {
		rgb = mix(rgb / 12.92, pow((rgb + 0.055) / 1.055, vec3(2.4)),
			greaterThan(rgb, vec3(0.04045)));
	}
	return vec4(pow(rgb, vec3(1.0 / 2.2)) * color.a, color.a);
}
void main() {
	vec4 sum = sample_blur(uv) * 4.0;
	sum += sample_blur(uv - halfpixel * offset);
	sum += sample_blur(uv + halfpixel * offset);
	sum += sample_blur(uv + vec2(halfpixel.x, -halfpixel.y) * offset);
	sum += sample_blur(uv - vec2(halfpixel.x, -halfpixel.y) * offset);
	out_color = sum * 0.125;
}
