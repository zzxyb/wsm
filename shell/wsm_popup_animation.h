#ifndef WSM_POPUP_ANIMATION_H
#define WSM_POPUP_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

struct wlr_scene_tree;

enum wsm_popup_animation_direction {
	WSM_POPUP_ANIMATION_FROM_LEFT,
	WSM_POPUP_ANIMATION_FROM_RIGHT,
	WSM_POPUP_ANIMATION_FROM_TOP,
	WSM_POPUP_ANIMATION_FROM_BOTTOM,
};

struct wsm_popup_animation_options {
	enum wsm_popup_animation_direction direction;
	uint32_t duration_msec;
	int travel;
};

bool wsm_popup_animation_start(struct wlr_scene_tree *tree,
	const struct wsm_popup_animation_options *options);

#endif
