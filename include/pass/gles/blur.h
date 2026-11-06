/**
 * @file        blur.h
 * @brief       GLES2 blur render pass resources.
 * @details     Defines Dual Kawase shaders, intermediate render textures,
 *              and backend render helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */

#ifndef EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H

#include "pass/blur.h"


#include <GLES2/gl2.h>

/**
 * @brief Dual Kawase downsampling shader locations.
 */
struct wsm_gles_blur_down_shader {
	GLuint program; /**< Linked shader program. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint tex_proj; /**< Source texture projection uniform location. */
	GLint halfpixel; /**< Half source texel size uniform location. */
	GLint offset; /**< Sampling offset uniform location. */
	GLint pos; /**< Vertex position attribute location. */
};

/**
 * @brief Dual Kawase upsampling shader locations.
 */
struct wsm_gles_blur_up_shader {
	GLuint program; /**< Linked shader program. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint halfpixel; /**< Half source texel size uniform location. */
	GLint offset; /**< Sampling offset uniform location. */
	GLint pos; /**< Vertex position attribute location. */
};

/**
 * @brief Appearance shader for the filtered texture.
 */
struct wsm_gles_blur_round_shader {
	GLuint program; /**< Linked shader program. */
	GLint proj; /**< Destination projection uniform location. */
	GLint tex_proj; /**< Source texture projection uniform location. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint pos; /**< Vertex position attribute location. */
	GLint size; /**< Destination size uniform location. */
	GLint saturation; /**< Saturation multiplier uniform location. */
	GLint brightness; /**< Brightness multiplier uniform location. */
	GLint noise_amount; /**< Noise amplitude uniform location. */
	GLint material_contrast; /**< Highlight and shadow adjustment uniform location. */
	GLint tint; /**< Straight RGBA tint uniform location. */
};

/**
 * @brief Reusable intermediate texture and its framebuffer.
 */
struct wsm_gles_blur_texture {
	GLuint tex; /**< Owned GL texture name. */
	GLuint fbo; /**< Framebuffer attached to tex. */
	int width; /**< Texture width in pixels. */
	int height; /**< Texture height in pixels. */
};

/**
 * @brief Renderer-embedded blur shaders and reusable texture pyramid.
 */
struct wsm_gles2_blur {
	struct {
		struct wsm_gles_blur_down_shader rgba; /**< Downsampling shader for regular 2D textures. */
		struct wsm_gles_blur_down_shader ext; /**< Optional downsampling shader for external textures. */
	} down; /**< Source-specific downsampling shaders. */
	struct wsm_gles_blur_up_shader up; /**< Upsampling shader. */
	struct wsm_gles_blur_round_shader round; /**< Filtered texture appearance shader. */
	struct wsm_gles2_texture *filtered; /**< Reusable texture for final rounded composition. */
	struct wsm_gles_blur_texture levels[WSM_BLUR_BUFFER_MAX_PASSES + 1]; /**< Sampling texture levels. */
};

struct wsm_gles2_renderer;

/** Initializes blur shaders embedded in the renderer. The EGL context must be current. */
bool wsm_gles2_blur_init(struct wsm_gles2_renderer *renderer);

/** Releases embedded blur resources. The EGL context must be current. */
void wsm_gles2_blur_finish(struct wsm_gles2_renderer *renderer);

/** Filters and composites a texture on the active GLES2 render pass. */
void wsm_gles2_render_pass_add_blur(struct wlr_render_pass *pass,
	const struct wsm_render_blur_buffer_options *options);

/** Creates or updates a snapshot of the active GLES2 render target. */
struct wlr_texture *wsm_gles2_render_pass_capture_blur(struct wlr_render_pass *pass,
	struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage);

#endif // EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H
