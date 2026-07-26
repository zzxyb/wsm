#ifndef WSM_SCENE_CAPTURE_H
#define WSM_SCENE_CAPTURE_H

#include <stdbool.h>

#include <wlr/util/box.h>

struct wlr_allocator;
struct wlr_buffer;
struct wlr_renderer;
struct wlr_scene_tree;

struct wsm_scene_capture {
	struct wlr_buffer *buffer;
	struct wlr_box box;
	float scale;
};

bool wsm_scene_capture_tree(struct wsm_scene_capture *capture,
	struct wlr_scene_tree *tree, struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, float scale);

void wsm_scene_capture_finish(struct wsm_scene_capture *capture);

#endif
