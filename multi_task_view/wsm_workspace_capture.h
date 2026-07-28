#ifndef WSM_WORKSPACE_CAPTURE_H
#define WSM_WORKSPACE_CAPTURE_H

#include <stdbool.h>

#include <wlr/util/box.h>

struct wlr_allocator;
struct wlr_renderer;
struct wlr_scene_tree;
struct wsm_output;
struct wsm_workspace;

struct wsm_workspace_capture;

/**
 * Create a live, flattened workspace preview. The source workspace is never
 * enabled or reparented: its scene buffers are sampled directly into an
 * internal swapchain and exposed as a single scene buffer below @p parent.
 */
struct wsm_workspace_capture *wsm_workspace_capture_create(
	struct wlr_scene_tree *parent, struct wsm_output *output,
	struct wsm_workspace *workspace, const struct wlr_box *source_box,
	const struct wlr_box *destination, struct wlr_renderer *renderer,
	struct wlr_allocator *allocator);

struct wsm_workspace_capture *wsm_workspace_capture_create_options(
	struct wlr_scene_tree *parent, struct wsm_output *output,
	struct wsm_workspace *workspace, const struct wlr_box *source_box,
	const struct wlr_box *destination, struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, bool include_shell_layers,
	float buffer_scale);

struct wsm_workspace_capture *wsm_workspace_capture_create_layer_options(
	struct wlr_scene_tree *parent, struct wsm_output *output,
	struct wsm_workspace *workspace, const struct wlr_box *source_box,
	const struct wlr_box *destination, struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, bool include_shell_layers,
	bool include_shell_top, bool ignore_shell_lower_root_enabled,
	float buffer_scale);

/** Render pending source damage. Intended to run from the output frame event. */
bool wsm_workspace_capture_render(struct wsm_workspace_capture *capture);

bool wsm_workspace_capture_is_dirty(
	const struct wsm_workspace_capture *capture);

void wsm_workspace_capture_set_opacity(
	struct wsm_workspace_capture *capture, float opacity);

void wsm_workspace_capture_destroy(struct wsm_workspace_capture *capture);

#endif
