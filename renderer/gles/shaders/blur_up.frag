#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
uniform sampler2D tex;
uniform vec2 halfpixel;
uniform float offset;

void main() {
	vec4 sum = vec4(0.0);
	sum += texture2D(tex, v_texcoord + vec2(-halfpixel.x * 2.0, 0.0) * offset);
	sum += texture2D(tex, v_texcoord + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
	sum += texture2D(tex, v_texcoord + vec2(0.0, halfpixel.y * 2.0) * offset);
	sum += texture2D(tex, v_texcoord + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
	sum += texture2D(tex, v_texcoord + vec2(halfpixel.x * 2.0, 0.0) * offset);
	sum += texture2D(tex, v_texcoord + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
	sum += texture2D(tex, v_texcoord + vec2(0.0, -halfpixel.y * 2.0) * offset);
	sum += texture2D(tex, v_texcoord + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
	gl_FragColor = sum * 0.0833333333;
}
