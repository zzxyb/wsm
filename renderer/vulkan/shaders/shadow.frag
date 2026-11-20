#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 48) vec4 color;
	vec4 slices;
	vec4 radius;
	vec2 size;
	float corner_smoothness;
	float inset;
	float spread;
	float inner_offset;
} data;
#define size data.size
#define slices data.slices
#define radius data.radius
#define corner_smoothness data.corner_smoothness
#define inset data.inset
#define spread data.spread
#define inner_offset data.inner_offset
#define color data.color

float smoothRoundedRectSDF(vec2 p, vec2 half_size, float rad, float n) {
	vec2 q = abs(p) - half_size + rad;
	vec2 outside = max(q, 0.0);
	float norm = pow(pow(outside.x, n) + pow(outside.y, n), 1.0 / n);
	/* Euclidean distance along the ray to the superellipse boundary.
	 * One continuous expression also covers a zero-radius corner. */
	float euclidean = length(outside);
	float radial_scale = (euclidean + 0.001) / (norm + 0.001);
	float corner = euclidean - rad * radial_scale;
	float inside = min(max(q.x, q.y), 0.0);
	return corner + inside;
}

void main() {
	vec2 content_pos = vec2(slices.x, slices.z);
	vec2 content_size = size - vec2(slices.x + slices.y, slices.z + slices.w);
	vec2 p = uv * size - content_pos - content_size * 0.5;
	vec2 half_size = content_size * 0.5;
	vec2 quadrant = step(vec2(0.0), p);
	float top = mix(radius.x, radius.y, quadrant.x);
	float bottom = mix(radius.z, radius.w, quadrant.x);
	float rad = min(mix(top, bottom, quadrant.y),
		min(half_size.x, half_size.y));
	float dist = smoothRoundedRectSDF(p, half_size, rad, corner_smoothness);

	float d = max(dist, 0.0);
	float core = exp(-0.5 * (d * d) / (inset * inset)) * 0.42;
	float ambient = exp(-0.5 * (d * d) / (spread * spread)) * 0.26;
	float alpha = core + ambient;
	alpha *= smoothstep(-inner_offset, 1.0 - inner_offset, dist);

	float shadow_extent = min(min(slices.x, slices.y), min(slices.z, slices.w));
	float fade_end = max(shadow_extent - 2.0, 0.0);
	float fade_start = max(fade_end - spread * 1.25, 0.0);
	alpha *= 1.0 - smoothstep(fade_start, fade_end, d);

	vec2 pixel = uv * size;
	float edge_dist = min(min(pixel.x, size.x - pixel.x),
		min(pixel.y, size.y - pixel.y));
	alpha *= smoothstep(0.0, 8.0, edge_dist);

	alpha *= color.a;
	out_color = vec4(color.rgb * alpha, alpha);
}
