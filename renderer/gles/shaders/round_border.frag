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
uniform vec4 color;
/* top_left, top_right, bottom_left, bottom_right corner radii in pixels */
uniform vec4 radius;
/* rectangle dimensions in pixels */
uniform vec2 size;
uniform float border_width;
/* superellipse exponent for smoother, Apple-style rounded rectangles */
uniform float corner_smoothness;

/* Select the per-corner radius for p's quadrant without dynamic branching. */
float cornerRadius(vec2 p, vec4 r) {
	vec2 quadrant = step(vec2(0.0), p);
	float top = mix(r.x, r.y, quadrant.x);
	float bottom = mix(r.z, r.w, quadrant.x);
	return mix(top, bottom, quadrant.y);
}

/*
 * Approximate signed distance field for a smooth rounded rectangle.
 *   p         - position relative to rectangle centre, in pixels
 *   half_size - (width/2, height/2)
 *   r         - per-corner radii: x=top_left, y=top_right,
 *                                 z=bottom_left, w=bottom_right
 *   n         - superellipse exponent; 4.0 is a practical default
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
	vec2 p = (v_texcoord - 0.5) * size;

	float outer_dist = smoothRoundedRectSDF(p, half_size, radius, corner_smoothness);
	float width = clamp(border_width, 0.0, min(half_size.x, half_size.y));
	vec2 inner_half_size = max(half_size - vec2(width), vec2(0.0));
	vec4 inner_radius = max(radius - vec4(width), vec4(0.0));
	float inner_dist = smoothRoundedRectSDF(p, inner_half_size,
		inner_radius, corner_smoothness);

#ifdef HAS_DERIVATIVES
	float aa = max(fwidth(outer_dist), 1.0);
	float inner_aa = max(fwidth(inner_dist), 1.0);
#else
	float aa = 1.0;
	float inner_aa = 1.0;
#endif
	aa *= cornerAxisAAPinch(p, half_size, radius);
	inner_aa *= cornerAxisAAPinch(p, inner_half_size, inner_radius);

	float outer_alpha = 1.0 - smoothstep(-aa, aa, outer_dist);
	float inner_alpha = smoothstep(-inner_aa, inner_aa, inner_dist);
	float alpha = outer_alpha * inner_alpha;

	gl_FragColor = color * alpha;
}
