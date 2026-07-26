#ifndef WSM_WINDOW_ANIMATION_H
#define WSM_WINDOW_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#include <wlr/util/box.h>

struct wsm_container;

enum wsm_window_animation_kind {
	WSM_WINDOW_ANIMATION_CLOSE_DOOR,
	WSM_WINDOW_ANIMATION_OPEN_DOOR,
	WSM_WINDOW_ANIMATION_MINIMIZE,
	WSM_WINDOW_ANIMATION_RESTORE_MINIMIZE,
	WSM_WINDOW_ANIMATION_GEOMETRY,
};

struct wsm_window_animation_options {
	enum wsm_window_animation_kind kind;
	uint32_t duration_msec;
	float scale;
	struct wlr_box target_box;
};

bool wsm_window_animation_start_close(struct wsm_container *container,
	const struct wsm_window_animation_options *options);
bool wsm_window_animation_start_open(struct wsm_container *container,
	const struct wsm_window_animation_options *options);
bool wsm_window_animation_start_minimize(struct wsm_container *container,
	const struct wsm_window_animation_options *options);
bool wsm_window_animation_start_restore_minimize(struct wsm_container *container,
	const struct wsm_window_animation_options *options);
bool wsm_window_animation_start_geometry(struct wsm_container *container,
	const struct wsm_window_animation_options *options);

#endif
