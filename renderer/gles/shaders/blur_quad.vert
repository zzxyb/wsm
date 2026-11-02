attribute vec2 pos;
varying vec2 v_texcoord;

void main() {
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
	v_texcoord = pos;
}
