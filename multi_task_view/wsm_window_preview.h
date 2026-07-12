#ifndef WSM_WINDOW_PREVIEW_H
#define WSM_WINDOW_PREVIEW_H

#include <stdbool.h>

struct wlr_box;
struct wlr_scene_tree;
struct wsm_window_preview;

/**
 * Create a lightweight live mirror of the client buffers below source_tree.
 * source_x/source_y locate source_tree within the window's outer geometry.
 */
struct wsm_window_preview *wsm_window_preview_create(
	struct wlr_scene_tree *parent, struct wlr_scene_tree *source_tree,
	int source_width, int source_height, int source_x, int source_y);

/** Move and scale all mirrored client buffers into the supplied window box. */
void wsm_window_preview_set_geometry(struct wsm_window_preview *preview,
	const struct wlr_box *box);

/** Destroy all mirrors and detach their surface listeners. */
void wsm_window_preview_destroy(struct wsm_window_preview *preview);

#endif
