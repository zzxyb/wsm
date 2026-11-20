#version 450
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 48) vec4 tint;
	float saturation;
	float brightness;
	float noise_amount;
	float material_contrast;
} data;
#define tint data.tint
#define saturation data.saturation
#define brightness data.brightness
#define noise_amount data.noise_amount
#define material_contrast data.material_contrast
float grain(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
	vec4 color = texture(tex, uv);
	float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	color.rgb = mix(vec3(luma), color.rgb, saturation) * brightness;
	color.rgb = mix(color.rgb, tint.rgb * color.a, tint.a);
	luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	float contrast_dir = smoothstep(0.50, 0.95, luma) -
		(1.0 - smoothstep(0.05, 0.50, luma));
	color.rgb -= vec3(contrast_dir * material_contrast * color.a);
	float noise = grain(floor(gl_FragCoord.xy)) - 0.5;
	color.rgb += vec3(noise * noise_amount * color.a);
	color.rgb = clamp(color.rgb, vec3(0.0), vec3(color.a));

	out_color = color;
}
