#ifndef EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H

#include "pass/blur.h"

#include "renderer/gles/renderer.h"
#include "pass/gles/pass.h"
#include "texture/gles/texture.h"

#include <GLES2/gl2.h>

struct wsm_gles_blur_down_shader {
	GLuint program;
	GLint tex;
	GLint tex_proj;
	GLint halfpixel;
	GLint offset;
	GLint pos;
};

struct wsm_gles_blur_up_shader {
	GLuint program;
	GLint tex;
	GLint halfpixel;
	GLint offset;
	GLint pos;
};

struct wsm_gles_blur_round_shader {
	GLuint program;
	GLint proj;
	GLint tex_proj;
	GLint tex;
	GLint alpha;
	GLint pos;
	GLint radius;
	GLint size;
	GLint corner_smoothness;
	GLint saturation;
	GLint brightness;
	GLint noise_amount;
	GLint material_contrast;
	GLint tint;
};

struct wsm_gles_blur_texture {
	GLuint tex;
	GLuint fbo;
	int width;
	int height;
};

struct wsm_gles_render_blur_buffer_pass {
	struct wsm_render_blur_buffer_pass base;
	struct wsm_gles2_renderer *renderer;
	struct {
		struct wsm_gles_blur_down_shader rgba;
		struct wsm_gles_blur_down_shader ext;
	} down;
	struct wsm_gles_blur_up_shader up;
	struct wsm_gles_blur_round_shader round;
	struct wsm_gles_blur_texture levels[WSM_BLUR_BUFFER_MAX_PASSES + 1];
};

struct wsm_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_create(
	struct wlr_renderer *wlr_renderer);
bool wsm_render_blur_buffer_pass_is_gles2(const struct wsm_render_blur_buffer_pass *pass);
struct wsm_gles_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_from_pass(
	struct wsm_render_blur_buffer_pass *pass);

#endif
