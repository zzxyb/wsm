#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
varying vec2 v_pos;
uniform sampler2D tex;
uniform vec2 size;
uniform float saturation;
uniform float brightness;
uniform float noise_amount;
uniform float material_contrast;
uniform vec4 tint;

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

	gl_FragColor = color;
}
