#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform UBO {
	layout(offset = 48) vec4 color;
	vec4 radius;
	vec2 size;
	float corner_smoothness;
} data;
#define color data.color
#define radius data.radius
#define size data.size
#define corner_smoothness data.corner_smoothness

/* Select the per-corner radius for p's quadrant without dynamic branching. */
float cornerRadius(vec2 p, vec4 r) {
	vec2 quadrant = step(vec2(0.0), p);
	float top = mix(r.x, r.y, quadrant.x);
	float bottom = mix(r.z, r.w, quadrant.x);
	return mix(top, bottom, quadrant.y);
}

/*
 * Approximate signed distance field for a smooth rounded rectangle.
 *   p         – position relative to rectangle centre, in pixels
 *   half_size – (width/2, height/2)
 *   r         – per-corner radii: x=top_left, y=top_right,
 *                                 z=bottom_left, w=bottom_right
 *   n         – superellipse exponent; 4.0 is a practical default
 * Returns < 0 inside, 0 on the edge, > 0 outside.
 */
float smoothRoundedRectSDF(vec2 p, vec2 half_size, vec4 r, float n) {
	/* Pick radius for the quadrant this fragment falls in */
	float rad = cornerRadius(p, r);
	vec2 q = abs(p) - half_size + rad;
	vec2 outside = max(q, 0.0);
	float corner = pow(pow(outside.x, n) + pow(outside.y, n), 1.0 / n) - rad;
	float inside = min(max(q.x, q.y), 0.0);
	return corner + inside;
}

float cornerAxisAAPinch(vec2 p, vec2 half_size, vec4 r) {
	float rad = cornerRadius(p, r);

	vec2 q = abs(p) - half_size + rad;
	vec2 dir = abs(q) / max(rad, 0.0001);
	float axis = 1.0 - min(dir.x, dir.y);
	return mix(1.0, 0.55, smoothstep(0.55, 0.98, axis));
}

void main() {
	vec2 half_size = size * 0.5;
	/* Convert UV [0,1] → pixel-space coordinates relative to rectangle centre */
	vec2 p = (uv - 0.5) * size;

	float dist = smoothRoundedRectSDF(p, half_size, radius, corner_smoothness);


	/* Keep the AA ramp in screen pixels, including transformed/scaled outputs. */
	float aa = max(fwidth(dist), 1.0);

	aa *= cornerAxisAAPinch(p, half_size, radius);
	float alpha = 1.0 - smoothstep(-aa, aa, dist);

	/* color is premultiplied; scale all channels by the AA factor */
	out_color = color * alpha;
}
