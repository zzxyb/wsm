#ifndef WSM_SCENE_BLUR_H
#define WSM_SCENE_BLUR_H

#include "pass/blur.h"

#include <pixman.h>
#include <stdbool.h>
#include "node/wsm_scene_node.h"

/* A rounded, non-input backdrop filter over preceding scene content.
 * The effect pass is borrowed; renderer/pass destruction detaches it safely.
 * Background captures are cached per output and updated from capture damage.
 * Occluded source content is retained for filtering, without full-output repaint. */
struct wsm_scene_blur {
	struct wsm_scene_node node;
	struct wsm_render_blur_buffer_pass *pass;
	struct wl_listener pass_destroy;
	struct wl_list captures; /**< Per-output raw background caches. */
	int width;
	int height;
	float top_left_radius;
	float top_right_radius;
	float bottom_left_radius;
	float bottom_right_radius;
	int iterations;
	float offset;
	float saturation;
	float brightness;
	float noise_amount;
	float material_contrast;
	float opacity;
	float tint[4];
};

struct wsm_scene_blur *wsm_scene_blur_create(struct wsm_scene_tree *parent, int width, int height,
	float top_left_radius, float top_right_radius, float bottom_left_radius,
	float bottom_right_radius);
void wsm_scene_blur_set_size(struct wsm_scene_blur *blur, int width, int height);
void wsm_scene_blur_set_corner_radius(struct wsm_scene_blur *blur, float top_left, float top_right,
	float bottom_left, float bottom_right);
void wsm_scene_blur_set_pass(struct wsm_scene_blur *blur, struct wsm_render_blur_buffer_pass *pass);
void wsm_scene_blur_set_blur(struct wsm_scene_blur *blur, int iterations, float offset);
void wsm_scene_blur_set_appearance(
	struct wsm_scene_blur *blur, float saturation, float brightness, const float tint[4]);
void wsm_scene_blur_set_noise(struct wsm_scene_blur *blur, float noise_amount);
void wsm_scene_blur_set_material_contrast(struct wsm_scene_blur *blur, float contrast);
void wsm_scene_blur_add_capture_damage(
	struct wsm_scene_blur *blur, const pixman_region32_t *damage);
int wsm_scene_blur_get_damage_expansion(const struct wsm_scene_blur *blur, float scale);
/* Propagate source damage through this filter, preparing its capture cache.
 * damage is in output-buffer coordinates; called back-to-front by the output. */
/* Call before scene_output_commit/build_state, as in the reference demo.
 * Multiple blur nodes must be prepared in their scene order, back to front. */
bool wsm_scene_blur_prepare_output(struct wsm_scene_blur *blur, struct wsm_scene_output *output);

bool wsm_scene_blur_prepare(struct wsm_render_list_entry *entry, const struct wsm_render_data *data,
	pixman_region32_t *damage);

bool wsm_scene_node_is_blur(const struct wsm_scene_node *node);
struct wsm_scene_blur *wsm_scene_blur_from_node(struct wsm_scene_node *node);

#endif
