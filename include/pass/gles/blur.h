/**
 * @file        blur.h
 * @brief       GLES2 blur render pass resources.
 * @details     Defines Dual Kawase shaders, intermediate render textures,
 *              and backend render helpers.
 * @author      YaoBing Xiao
 * @date        2026-11-02
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-02, initial version\n
 */

#ifndef PASS_GLES_BLUR_H
#define PASS_GLES_BLUR_H

#include "pass/blur.h"


#include <GLES2/gl2.h>

/**
 * @brief Dual Kawase downsampling program and shader locations.
 */
struct wsm_gles_blur_down_shader {
	GLuint program; /**< Linked shader program. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint tex_proj; /**< Source texture projection uniform location. */
	GLint halfpixel; /**< Uniform location for half a texel in normalized source coordinates. */
	GLint offset; /**< Sampling offset uniform location. */
	GLint pos; /**< Vertex position attribute location. */
};

/**
 * @brief Dual Kawase upsampling program and shader locations.
 */
struct wsm_gles_blur_up_shader {
	GLuint program; /**< Linked shader program. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint halfpixel; /**< Uniform location for half a texel in normalized source coordinates. */
	GLint offset; /**< Sampling offset uniform location. */
	GLint pos; /**< Vertex position attribute location. */
};

/**
 * @brief Program and shader locations for adjusting the filtered texture.
 */
struct wsm_gles_blur_round_shader {
	GLuint program; /**< Linked shader program. */
	GLint proj; /**< Destination projection uniform location. */
	GLint tex_proj; /**< Source texture projection uniform location. */
	GLint tex; /**< Source texture sampler uniform location. */
	GLint pos; /**< Vertex position attribute location. */
	GLint size; /**< Uniform location for the destination size in pixels. */
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
	GLuint fbo; /**< Owned framebuffer with tex as its color attachment. */
	int width; /**< Texture width in pixels. */
	int height; /**< Texture height in pixels. */
};

/**
 * @brief Renderer-embedded blur shaders and reusable texture pyramid.
 */
struct wsm_gles2_blur {
	struct {
		struct wsm_gles_blur_down_shader rgba; /**< Downsampling shader for regular 2D textures. */
		struct wsm_gles_blur_down_shader ext; /**< Downsampling shader for GL_TEXTURE_EXTERNAL_OES when supported. */
	} down; /**< Source-specific downsampling shaders. */
	struct wsm_gles_blur_up_shader up; /**< Upsampling shader. */
	struct wsm_gles_blur_round_shader round; /**< Filtered texture appearance shader. */
	struct wsm_gles2_texture *filtered; /**< Reusable destination-sized texture holding the appearance-adjusted result. */
	struct wsm_gles_blur_texture levels[WSM_BLUR_BUFFER_MAX_PASSES + 1]; /**< Intermediate textures ordered from largest to smallest. */
};

struct wsm_gles2_renderer;

/**
 * @brief Links the renderer's blur programs and looks up their shader locations.
 * @details The renderer's EGL context must be current. If initialization fails,
 *          call wsm_gles2_blur_finish() to release any programs already linked.
 * @param renderer Renderer whose blur state is initialized.
 * @return true on success, false if program linking or location lookup fails.
 */
bool wsm_gles2_blur_init(struct wsm_gles2_renderer *renderer);

/**
 * @brief Releases blur programs and textures and resets the renderer's blur state.
 * @details The renderer's EGL context must be current.
 * @param renderer Renderer whose blur resources are released.
 */
void wsm_gles2_blur_finish(struct wsm_gles2_renderer *renderer);

/**
 * @brief Applies Dual Kawase blur and composites the result onto the render target.
 * @param pass Active GLES2 render pass receiving the blurred texture.
 * @param options Borrowed blur options with a source texture from the pass renderer.
 */
void wsm_gles2_render_pass_add_blur(struct wlr_render_pass *pass,
	const struct wsm_render_blur_buffer_options *options);

/**
 * @brief Copies pixels from the active render target into a snapshot texture.
 * @details Copies only the intersection of the snapshot extent, target bounds,
 *          and damage region. Copied pixels are stored relative to the snapshot
 *          origin. The caller owns the returned texture.
 * @param pass Active GLES2 render pass providing the source pixels.
 * @param previous Existing snapshot from the same renderer with matching dimensions,
 *                 or NULL to allocate a new texture.
 * @param box Snapshot extent in render-target pixels, or NULL for the whole target.
 * @param damage Region to copy in render-target coordinates, or NULL for the full extent.
 * @return The updated or newly allocated snapshot, or NULL on failure.
 */
struct wlr_texture *wsm_gles2_render_pass_capture_blur(struct wlr_render_pass *pass,
	struct wlr_texture *previous, const struct wlr_box *box, const pixman_region32_t *damage);

#endif // PASS_GLES_BLUR_H
