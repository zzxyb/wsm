#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

varying vec2 v_pos;
uniform vec4 radius;
uniform vec2 size;

float cornerCoverage() {
	vec2 p = (v_pos - 0.5) * size;
	vec2 quadrant = step(vec2(0.0), p);
	float r = mix(mix(radius.x, radius.y, quadrant.x),
		mix(radius.z, radius.w, quadrant.x), quadrant.y);
	r = clamp(r, 0.0, min(size.x, size.y) * 0.5);
	vec2 q = max(abs(p) - size * 0.5 + r, 0.0);
	float corner = sign(r) * sign(q.x) * sign(q.y);
	float unit = max(r, 1.0);
	vec2 normalized = q / unit;
	float dist = pow(pow(normalized.x, 4.0) + pow(normalized.y, 4.0), 0.25) * unit - r;
	return mix(1.0, 1.0 - smoothstep(-1.0, 1.0, dist), corner);
}

void main() {
	gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha * cornerCoverage();
}
