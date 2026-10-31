#ifdef GL_OES_standard_derivatives
#extension GL_OES_standard_derivatives : enable
#define HAS_DERIVATIVES 1
#endif

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
varying vec2 v_pos;
uniform sampler2D tex;
uniform float alpha;
uniform vec4 radius;
uniform vec2 size;
uniform float corner_smoothness;
uniform float saturation;
uniform float brightness;
uniform float noise_amount;
uniform float material_contrast;
uniform vec4 tint;

float smoothRoundedRectSDF(vec2 p, vec2 half_size, vec4 r, float n) {
	vec2 quadrant = step(vec2(0.0), p);
	float rad = mix(mix(r.x, r.y, quadrant.x), mix(r.z, r.w, quadrant.x), quadrant.y);
	vec2 q = abs(p) - half_size + rad;
	vec2 outside = max(q, 0.0);
	float corner = pow(pow(outside.x, n) + pow(outside.y, n), 1.0 / n) - rad;
	float inside = min(max(q.x, q.y), 0.0);
	return corner + inside;
}

float grain(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
	vec4 color = texture2D(tex, v_texcoord);
	float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	color.rgb = mix(vec3(luma), color.rgb, saturation) * brightness;
	color.rgb = mix(color.rgb, tint.rgb * color.a, tint.a);
	luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	float contrast_dir = smoothstep(0.50, 0.95, luma) -
		(1.0 - smoothstep(0.05, 0.50, luma));
	color.rgb -= vec3(contrast_dir * material_contrast * color.a);
	float noise = grain(floor(v_pos * size)) - 0.5;
	color.rgb += vec3(noise * noise_amount * color.a);
	color.rgb = clamp(color.rgb, vec3(0.0), vec3(color.a));

	vec2 half_size = size * 0.5;
	vec2 p = (v_pos - 0.5) * size;
	float dist = smoothRoundedRectSDF(p, half_size, radius, corner_smoothness);
#ifdef HAS_DERIVATIVES
	float aa = max(fwidth(dist), 1.0);
#else
	float aa = 1.0;
#endif
	float mask = 1.0 - smoothstep(-aa, aa, dist);
	gl_FragColor = color * alpha * mask;
}
