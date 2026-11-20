#version 450
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 48) vec2 halfpixel;
	float offset;
} data;
#define halfpixel data.halfpixel
#define offset data.offset
void main() {
	vec4 sum = vec4(0.0);
	sum += texture(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
	sum += texture(tex, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
	sum += texture(tex, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
	sum += texture(tex, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
	sum += texture(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
	sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
	sum += texture(tex, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
	sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
	out_color = sum * 0.0833333333;
}
