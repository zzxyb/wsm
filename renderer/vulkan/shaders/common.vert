#version 450

layout(push_constant) uniform UBO {
	// Projection matrix in packed form. Using mat3 would use 12 floats due to
	// aligment, and push constant memory is very limited on most devices.
	float proj_packed[6];
	vec2 uv_offset;
	vec2 uv_size;
	vec2 padding;
} data;

layout(location = 0) in vec4 inst_rect;

layout(location = 0) out vec2 uv;

void main() {
	mat3 proj = mat3(data.proj_packed[0], data.proj_packed[3], 0,
		data.proj_packed[1], data.proj_packed[4], 0,
		data.proj_packed[2], data.proj_packed[5], 1);
	vec2 pos = vec2(float((gl_VertexIndex + 1) & 2) * 0.5f,
		float(gl_VertexIndex & 2) * 0.5f);
	pos = inst_rect.xy + pos * inst_rect.zw;
	uv = data.uv_offset + pos * data.uv_size;
	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
}
