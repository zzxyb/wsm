#ifndef WSM_SCENE_BLUR_BUFFER_H
#define WSM_SCENE_BLUR_BUFFER_H

#include "pass/blur.h"

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include "node/wsm_scene_buffer.h"
#include "node/wsm_scene_node.h"

struct wsm_scene_blur_buffer {
	struct wsm_scene_buffer buffer;
	struct wsm_render_blur_buffer_pass *pass;
	struct wl_listener pass_destroy;
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
	float tint[4];
};

/* Independent blur-buffer node, following the reference commit. Standard
 * buffer properties are available through the embedded buffer. Use the blur
 * setter below when replacing content so the filter's whole node is damaged. */
struct wsm_scene_blur_buffer *wsm_scene_blur_buffer_create(struct wsm_scene_tree *parent,
	struct wlr_buffer *buffer, struct wsm_render_blur_buffer_pass *pass,
	const struct wsm_render_blur_buffer_options *options);
void wsm_scene_blur_buffer_set_buffer(
	struct wsm_scene_blur_buffer *blur, struct wlr_buffer *buffer);
void wsm_scene_blur_buffer_set_pass(
	struct wsm_scene_blur_buffer *buffer, struct wsm_render_blur_buffer_pass *pass);
void wsm_scene_blur_buffer_set_blur(
	struct wsm_scene_blur_buffer *buffer, int iterations, float offset);
void wsm_scene_blur_buffer_set_appearance(
	struct wsm_scene_blur_buffer *buffer, float saturation, float brightness, const float tint[4]);
void wsm_scene_blur_buffer_set_noise(struct wsm_scene_blur_buffer *buffer, float noise_amount);
void wsm_scene_blur_buffer_set_material_contrast(
	struct wsm_scene_blur_buffer *buffer, float contrast);

bool wsm_scene_node_is_blur_buffer(const struct wsm_scene_node *node);
struct wsm_scene_blur_buffer *wsm_scene_blur_buffer_from_node(struct wsm_scene_node *node);

#endif
