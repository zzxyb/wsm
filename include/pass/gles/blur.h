/**
 * @file        blur.h
 * @brief       GLES2 blur render pass resources.
 * @details     Defines Dual Kawase shaders, intermediate render textures,
 *              and backend creation and conversion helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 */

#ifndef EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H

#include "pass/blur.h"

#include "renderer/gles/renderer.h"
#include "pass/gles/pass.h"
#include "texture/gles/texture.h"

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
 * @brief Final composition shader with rounded clipping and appearance controls.
 */
struct wsm_gles_blur_round_shader {
	GLuint program; /**< Linked shader program. */
	GLint proj; /**< Destination projection uniform location. */
	GLint tex_proj; /**< Source texture projection uniform location. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint alpha; /**< Opacity uniform location. */
	GLint pos; /**< Vertex position attribute location. */
	GLint radius; /**< Four corner radii uniform location. */
	GLint size; /**< Destination size uniform location. */
	GLint corner_smoothness; /**< Rounded corner exponent uniform location. */
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
 * @brief GLES2 blur pass with reusable shaders and texture pyramid.
 */
struct wsm_gles_render_blur_buffer_pass {
	struct wsm_render_blur_buffer_pass base; /**< Common blur pass state. */
	struct wsm_gles2_renderer *renderer; /**< GLES2 renderer owning the GL resources. */
	struct {
		struct wsm_gles_blur_down_shader rgba; /**< Downsampling shader for regular 2D textures. */
		struct wsm_gles_blur_down_shader ext; /**< Optional downsampling shader for external textures. */
	} down; /**< Source-specific downsampling shaders. */
	struct wsm_gles_blur_up_shader up; /**< Upsampling shader. */
	struct wsm_gles_blur_round_shader round; /**< Rounded final composition shader. */
	struct wsm_gles_blur_texture levels[WSM_BLUR_BUFFER_MAX_PASSES + 1]; /**< Sampling texture levels. */
};

/**
 * @brief Creates the GLES2 backend shaders and blur resources.
 * @details Called by wsm_render_blur_buffer_pass_create, which attaches the
 *          common renderer ownership and destruction listener.
 * @param wlr_renderer wsm GLES2 renderer that owns the GL resources.
 * @return New backend pass, or NULL on creation failure.
 */
struct wsm_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_create(
	struct wlr_renderer *wlr_renderer);

/**
 * @brief Checks whether a blur pass uses the GLES2 backend.
 * @param pass Non-NULL blur pass to inspect.
 * @return true for the GLES2 backend, false otherwise.
 */
bool wsm_render_blur_buffer_pass_is_gles2(const struct wsm_render_blur_buffer_pass *pass);

/**
 * @brief Casts a GLES2 blur pass to its backend object.
 * @param pass Blur pass known to use the GLES2 backend.
 * @return Enclosing GLES2 blur pass.
 */
struct wsm_gles_render_blur_buffer_pass *wsm_gles_render_blur_buffer_pass_from_pass(
	struct wsm_render_blur_buffer_pass *pass);

#endif // EFFECTS_GLES_WSM_BLUR_BUFFER_PASS_H
