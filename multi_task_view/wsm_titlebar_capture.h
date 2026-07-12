#ifndef WSM_TITLEBAR_CAPTURE_H
#define WSM_TITLEBAR_CAPTURE_H

#include <stdbool.h>

#include <wlr/util/box.h>

struct wlr_allocator;
struct wlr_buffer;
struct wlr_renderer;
struct wlr_scene_tree;

/** A one-shot, flattened image of a titlebar scene subtree. */
struct wsm_titlebar_capture {
	struct wlr_buffer *buffer;
	struct wlr_box box;
	float scale;
};

/**
 * Render all enabled rect and buffer descendants of @p tree into one buffer.
 * The returned box is expressed in tree-local logical coordinates.
 */
bool wsm_titlebar_capture(struct wsm_titlebar_capture *capture,
	struct wlr_scene_tree *tree, struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, float scale);

/** Drop the capture's ownership of its buffer. */
void wsm_titlebar_capture_finish(struct wsm_titlebar_capture *capture);

#endif
