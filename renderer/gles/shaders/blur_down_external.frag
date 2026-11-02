#extension GL_OES_EGL_image_external : require

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
uniform samplerExternalOES tex;
uniform mat3 tex_proj;
uniform vec2 halfpixel;
uniform float offset;

void main() {
	vec2 uv = (vec3(v_texcoord, 1.0) * tex_proj).xy;
	vec4 sum = texture2D(tex, uv) * 4.0;
	sum += texture2D(tex, uv - halfpixel * offset);
	sum += texture2D(tex, uv + halfpixel * offset);
	sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
	sum += texture2D(tex, uv - vec2(halfpixel.x, -halfpixel.y) * offset);
	gl_FragColor = sum * 0.125;
}
