uniform mat3 proj;
attribute vec2 pos;
varying vec2 v_texcoord;

void main() {
	vec3 pos3 = vec3(pos, 1.0);
	gl_Position = vec4(pos3 * proj, 1.0);
	/* Pass normalized (0..1) rect-space UV to the fragment shader */
	v_texcoord = pos;
}
