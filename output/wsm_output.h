#ifndef WSM_OUTPUT_H
#define WSM_OUTPUT_H

#include "node/wsm_node.h"

#include <bits/types/struct_timespec.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output_layout.h>

struct udev_device;

struct wlr_output;
struct wlr_scene_rect;
struct wlr_output_mode;
struct wlr_scene_output;
struct wlr_drm_connector;
struct wlr_output_power_v1_set_mode_event;

struct wsm_workspace;
struct wsm_workspace_manager;

enum scale_filter_mode {
	SCALE_FILTER_DEFAULT, // the default is currently smart
	SCALE_FILTER_LINEAR,
	SCALE_FILTER_NEAREST,
	SCALE_FILTER_SMART,
};

struct wsm_output_state {
	struct wsm_list *workspaces;
	struct wsm_workspace *active_workspace;
};

/**
 * @brief The wsm_output class
 */
struct wsm_output {
	struct {
		struct wlr_scene_tree *shell_background;
		struct wlr_scene_tree *shell_bottom;
		struct wlr_scene_tree *tiling;
		struct wlr_scene_tree *fullscreen;
		struct wlr_scene_tree *shell_top;
		struct wlr_scene_tree *shell_overlay;
		struct wlr_scene_tree *session_lock;
		struct wlr_scene_tree *osd;
		struct wlr_scene_tree *water_mark;
		struct wlr_scene_tree *black_screen;
	} layers;

	struct wsm_node node;

	struct wl_listener layout_destroy;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener present;
	struct wl_listener frame;
	struct wl_listener request_state;

	struct wl_list link;
	struct wlr_box usable_area;
	struct wsm_output_state current;

	struct {
		struct wl_signal disable;
	} events;

	struct timespec last_presentation;
	struct wsm_list *workspaces;

	struct wlr_scene_rect *fullscreen_background;

	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;

	struct wlr_color_transform *color_transform;

	struct wl_event_source *repaint_timer;

	uint32_t refresh_nsec;
	int max_render_time; // In milliseconds
	int lx, ly; // layout coords
	int width, height; // transformed buffer size
	enum wl_output_subpixel detected_subpixel;
	enum scale_filter_mode scale_filter;

	bool enabled;
	bool gamma_lut_changed;
	bool leased;
};

struct wsm_output_non_desktop {
	struct wl_listener destroy;
	struct wlr_output *wlr_output;
};

struct wsm_output *wsm_ouput_create(struct wlr_output *wlr_output);
void wsm_output_destroy(struct wsm_output *output);
void output_begin_destroy(struct wsm_output *output);
void output_enable(struct wsm_output *output);
void output_disable(struct wsm_output *output);
struct wsm_output *wsm_wsm_output_nearest_to_cursor();
struct wsm_output *wsm_output_nearest_to(int lx, int ly);
struct wsm_output *wsm_output_from_wlr_output(struct wlr_output *wlr_output);
struct wsm_output *wsm_output_get_in_direction(struct wsm_output *reference,
	enum wlr_direction direction);
void wsm_output_add_workspace(struct wsm_output *output,
	struct wsm_workspace *workspace);
void wsm_output_damage_whole(struct wsm_output *output);
void wsm_output_damage_surface(struct wsm_output *output, double ox, double oy,
	struct wlr_surface *surface, bool whole);
void wsm_output_damage_box(struct wsm_output *output, struct wlr_box *box);
struct wlr_box wsm_output_usable_area_in_layout_coords(struct wsm_output *output);
struct wlr_box wsm_output_usable_area_scaled(struct wsm_output *output);
void wsm_output_set_enable_adaptive_sync(struct wlr_output *output, bool enabled);

void wsm_output_power_manager_set_mode(struct wlr_output_power_v1_set_mode_event *event);

/**
 * @brief wsm_output_calculate_usable_area calculate unusable area on output
 * for example, the area occupied by the Dock
 * @param output
 * @param usable_area
 */
void wsm_output_calculate_usable_area(struct wsm_output *output, struct wlr_box *usable_area);
bool wsm_output_is_usable(struct wsm_output *output);
bool wsm_output_can_reuse_mode(struct wsm_output *output);
void output_get_box(struct wsm_output *output, struct wlr_box *box);
struct wsm_workspace *output_get_active_workspace(struct wsm_output *output);
struct wsm_output_non_desktop *output_non_desktop_create(struct wlr_output *wlr_output);
void output_for_each_container(struct wsm_output *output,
	void (*f)(struct wsm_container *con, void *data), void *data);
void request_modeset();
struct udev_device *wsm_output_get_device_handle(struct wsm_output *output);
struct wsm_output *output_by_name_or_id(const char *name_or_id);
void output_for_each_workspace(struct wsm_output *output,
	void (*f)(struct wsm_workspace *ws, void *data), void *data);

#endif
