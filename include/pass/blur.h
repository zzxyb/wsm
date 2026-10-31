#ifndef EFFECTS_WSM_BLUR_BUFFER_PASS_H
#define EFFECTS_WSM_BLUR_BUFFER_PASS_H

#include <wayland-server-core.h>

#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>

#include <pixman.h>

#define WSM_BLUR_BUFFER_MAX_PASSES 8

/* Blur uses Dual Kawase down/up sampling with bilinear filtering. Source,
 * destination, clip and radii are in render-target pixels. Zero iterations,
 * offset, saturation, brightness and corner_smoothness select the reference
 * defaults (4, 1.8, 1.35, 1.06 and 4). Tint is straight RGBA; textures and
 * render output use premultiplied alpha. Options are borrowed for the call. */
struct wsm_render_blur_buffer_options {
	struct wlr_texture *texture;
	/* Extra source pixels around dst_box, cropped after filtering. */
	int padding;
	struct wlr_fbox src_box;
	struct wlr_box dst_box;
	const float *alpha;
	const pixman_region32_t *clip;
	enum wl_output_transform transform;
	enum wlr_scale_filter_mode filter_mode;
	enum wlr_render_blend_mode blend_mode;
	float top_left_radius;
	float top_right_radius;
	float bottom_left_radius;
	float bottom_right_radius;
	float corner_smoothness;
	int iterations;
	float offset;
	float saturation;
	float brightness;
	float noise_amount;
	float material_contrast;
	float tint[4];
	struct wlr_drm_syncobj_timeline *wait_timeline;
	uint64_t wait_point;
};

struct wsm_render_blur_buffer_pass;

struct wsm_render_blur_buffer_pass_impl {
	struct wlr_texture *(*capture)(struct wlr_render_pass *target, struct wlr_texture *texture,
		const struct wlr_box *box, const pixman_region32_t *damage);
	void (*destroy)(struct wsm_render_blur_buffer_pass *pass);
	void (*render)(struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass,
		const struct wsm_render_blur_buffer_options *options);
};

/* Owned effect resources, automatically destroyed with their renderer. */
struct wsm_render_blur_buffer_pass {
	const struct wsm_render_blur_buffer_pass_impl *impl;
	struct wlr_renderer *renderer;
	struct wl_listener renderer_destroy;
	struct {
		struct wl_signal destroy;
	} events;
};

/* Returns an owned snapshot of the current target, or NULL on failure. */
struct wlr_texture *wsm_render_blur_buffer_pass_capture(
	struct wlr_render_pass *target, struct wsm_render_blur_buffer_pass *pass);

/* Reuse a raw background snapshot and update only target-buffer damage.
 * box selects its buffer-coordinate extent (NULL means the whole target).
 * A NULL texture allocates the snapshot; NULL damage copies the entire extent. */
bool wsm_render_blur_buffer_pass_update_capture(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass, struct wlr_texture **texture,
	const struct wlr_box *box, const pixman_region32_t *damage);

void wsm_render_blur_buffer_pass_init(
	struct wsm_render_blur_buffer_pass *pass, const struct wsm_render_blur_buffer_pass_impl *impl);
void wsm_render_blur_buffer_pass_destroy(struct wsm_render_blur_buffer_pass *pass);
struct wsm_render_blur_buffer_pass *wsm_render_blur_buffer_pass_create(
	struct wlr_renderer *renderer);
void wsm_render_blur_buffer_pass_render(struct wlr_render_pass *target,
	struct wsm_render_blur_buffer_pass *pass, const struct wsm_render_blur_buffer_options *options);

#endif
