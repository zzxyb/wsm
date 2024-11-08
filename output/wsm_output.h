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

/**
 * @brief Enumeration of scale filter modes for output rendering
 */
enum scale_filter_mode {
	SCALE_FILTER_DEFAULT, /**< The default filter mode, currently set to smart */
	SCALE_FILTER_LINEAR, /**< Linear scaling filter */
	SCALE_FILTER_NEAREST, /**< Nearest neighbor scaling filter */
	SCALE_FILTER_SMART, /**< Smart scaling filter */
};

/**
 * @brief Structure representing the state of an output
 */
struct wsm_output_state {
	struct wsm_list *workspaces; /**< List of workspaces associated with the output */
	struct wsm_workspace *active_workspace; /**< Pointer to the currently active workspace */
};

/**
 * @brief Structure representing an output in the WSM
 */
struct wsm_output {
	struct {
		struct wlr_scene_tree *shell_background; /**< Scene tree for the background layer */
		struct wlr_scene_tree *shell_bottom; /**< Scene tree for the bottom layer */
		struct wlr_scene_tree *tiling; /**< Scene tree for tiled windows */
		struct wlr_scene_tree *fullscreen; /**< Scene tree for fullscreen windows */
		struct wlr_scene_tree *shell_top; /**< Scene tree for the top layer */
		struct wlr_scene_tree *shell_overlay; /**< Scene tree for overlay layers */
		struct wlr_scene_tree *session_lock; /**< Scene tree for session lock */
		struct wlr_scene_tree *osd; /**< Scene tree for on-screen display */
		struct wlr_scene_tree *water_mark; /**< Scene tree for watermark */
		struct wlr_scene_tree *black_screen; /**< Scene tree for black screen */
	} layers;

	struct wsm_node node; /**< Node representing the output */

	struct wl_listener layout_destroy; /**< Listener for layout destruction events */
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wl_listener commit; /**< Listener for commit events */
	struct wl_listener present; /**< Listener for present events */
	struct wl_listener frame; /**< Listener for frame events */
	struct wl_listener request_state; /**< Listener for state request events */

	struct wl_list link; /**< Link for output list */
	struct wlr_box usable_area; /**< Usable area of the output */
	struct wsm_output_state current; /**< Current state of the output */

	struct {
		struct wl_signal disable; /**< Signal emitted when the output is disabled */
	} events;

	struct timespec last_presentation; /**< Timestamp of the last presentation */
	struct wsm_list *workspaces; /**< List of workspaces associated with the output */

	struct wlr_scene_rect *fullscreen_background; /**< Pointer to the fullscreen background rectangle */

	struct wlr_output *wlr_output; /**< Pointer to the WLR output instance */
	struct wlr_scene_output *scene_output; /**< Pointer to the WLR scene output instance */

	struct wlr_color_transform *color_transform; /**< Pointer to the color transform for the output */

	struct wl_event_source *repaint_timer; /**< Pointer to the repaint timer event source */

	uint32_t refresh_nsec; /**< Refresh rate in nanoseconds */
	int max_render_time; /**< Maximum render time in milliseconds */
	int lx, ly; /**< Layout coordinates */
	int width, height; /**< Transformed buffer size */
	enum wl_output_subpixel detected_subpixel; /**< Detected subpixel layout */
	enum scale_filter_mode scale_filter; /**< Current scale filter mode */

	bool enabled; /**< Flag indicating if the output is enabled */
	bool gamma_lut_changed; /**< Flag indicating if the gamma LUT has changed */
	bool leased; /**< Flag indicating if the output is leased */
};

/**
 * @brief Structure representing a non-desktop output
 */
struct wsm_output_non_desktop {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wlr_output *wlr_output; /**< Pointer to the WLR output instance */
};

/**
 * @brief Creates a new wsm_output instance
 * @param wlr_output Pointer to the WLR output instance to be associated with the new output
 * @return Pointer to the newly created wsm_output instance
 */
struct wsm_output *wsm_ouput_create(struct wlr_output *wlr_output);

/**
 * @brief Destroys the specified wsm_output instance
 * @param output Pointer to the wsm_output instance to be destroyed
 */
void wsm_output_destroy(struct wsm_output *output);

/**
 * @brief Begins the destruction process for the specified output
 * @param output Pointer to the wsm_output instance to begin destruction
 */
void output_begin_destroy(struct wsm_output *output);

/**
 * @brief Enables the specified output
 * @param output Pointer to the wsm_output instance to be enabled
 */
void output_enable(struct wsm_output *output);

/**
 * @brief Disables the specified output
 * @param output Pointer to the wsm_output instance to be disabled
 */
void output_disable(struct wsm_output *output);

/**
 * @brief Retrieves the output nearest to the current cursor position
 * @return Pointer to the nearest wsm_output instance
 */
struct wsm_output *wsm_output_nearest_to_cursor();

/**
 * @brief Retrieves the output nearest to the specified coordinates
 * @param lx X coordinate
 * @param ly Y coordinate
 * @return Pointer to the nearest wsm_output instance
 */
struct wsm_output *wsm_output_nearest_to(int lx, int ly);

/**
 * @brief Retrieves the wsm_output associated with the specified WLR output
 * @param wlr_output Pointer to the WLR output instance
 * @return Pointer to the associated wsm_output instance
 */
struct wsm_output *wsm_output_from_wlr_output(struct wlr_output *wlr_output);

/**
 * @brief Retrieves the output in the specified direction relative to a reference output
 * @param reference Pointer to the reference wsm_output
 * @param direction Direction to search for the output
 * @return Pointer to the wsm_output in the specified direction
 */
struct wsm_output *wsm_output_get_in_direction(struct wsm_output *reference,
	enum wlr_direction direction);

/**
 * @brief Adds a workspace to the specified output
 * @param output Pointer to the wsm_output to which the workspace will be added
 * @param workspace Pointer to the wsm_workspace to be added
 */
void wsm_output_add_workspace(struct wsm_output *output,
	struct wsm_workspace *workspace);

/**
 * @brief Damages the entire area of the specified output
 * @param output Pointer to the wsm_output to be damaged
 */
void wsm_output_damage_whole(struct wsm_output *output);

/**
 * @brief Damages a specific surface on the specified output
 * @param output Pointer to the wsm_output where the surface is located
 * @param ox X offset of the surface
 * @param oy Y offset of the surface
 * @param surface Pointer to the wlr_surface to be damaged
 * @param whole Boolean indicating if the whole surface should be damaged
 */
void wsm_output_damage_surface(struct wsm_output *output, double ox, double oy,
	struct wlr_surface *surface, bool whole);

/**
 * @brief Damages a specific box area on the specified output
 * @param output Pointer to the wsm_output where the box is located
 * @param box Pointer to the wlr_box to be damaged
 */
void wsm_output_damage_box(struct wsm_output *output, struct wlr_box *box);

/**
 * @brief Retrieves the usable area of the output in layout coordinates
 * @param output Pointer to the wsm_output whose usable area is to be retrieved
 * @return wlr_box representing the usable area in layout coordinates
 */
struct wlr_box wsm_output_usable_area_in_layout_coords(struct wsm_output *output);

/**
 * @brief Retrieves the scaled usable area of the output
 * @param output Pointer to the wsm_output whose usable area is to be retrieved
 * @return wlr_box representing the scaled usable area
 */
struct wlr_box wsm_output_usable_area_scaled(struct wsm_output *output);

/**
 * @brief Sets the adaptive sync enable state for the specified output
 * @param output Pointer to the wlr_output to be configured
 * @param enabled Boolean indicating if adaptive sync should be enabled
 */
void wsm_output_set_enable_adaptive_sync(struct wlr_output *output, bool enabled);

/**
 * @brief Sets the power management mode for the output
 * @param event Pointer to the event containing the mode information
 */
void wsm_output_power_manager_set_mode(struct wlr_output_power_v1_set_mode_event *event);

/**
 * @brief Calculates the unusable area on the output
 * For example, the area occupied by the Dock
 * @param output Pointer to the wsm_output for which to calculate unusable area
 * @param usable_area Pointer to the wlr_box to be populated with the unusable area
 */
void wsm_output_calculate_usable_area(struct wsm_output *output, struct wlr_box *usable_area);

/**
 * @brief Checks if the specified output is usable
 * @param output Pointer to the wsm_output to be checked
 * @return true if the output is usable, false otherwise
 */
bool wsm_output_is_usable(struct wsm_output *output);

/**
 * @brief Checks if the specified output can reuse its mode
 * @param output Pointer to the wsm_output to be checked
 * @return true if the output can reuse its mode, false otherwise
 */
bool wsm_output_can_reuse_mode(struct wsm_output *output);

/**
 * @brief Retrieves the bounding box of the specified output
 * @param output Pointer to the wsm_output whose box is to be retrieved
 * @param box Pointer to the wlr_box to be populated with the dimensions
 */
void output_get_box(struct wsm_output *output, struct wlr_box *box);

/**
 * @brief Retrieves the active workspace associated with the specified output
 * @param output Pointer to the wsm_output whose active workspace is to be retrieved
 * @return Pointer to the active wsm_workspace
 */
struct wsm_workspace *output_get_active_workspace(struct wsm_output *output);

/**
 * @brief Creates a new non-desktop output
 * @param wlr_output Pointer to the WLR output instance to be associated with the non-desktop output
 * @return Pointer to the newly created wsm_output_non_desktop instance
 */
struct wsm_output_non_desktop *output_non_desktop_create(struct wlr_output *wlr_output);

/**
 * @brief Executes a function for each container in the specified output
 * @param output Pointer to the wsm_output containing the containers
 * @param f Function to execute for each container
 * @param data Pointer to user data to be passed to the function
 */
void output_for_each_container(struct wsm_output *output,
	void (*f)(struct wsm_container *con, void *data), void *data);

/**
 * @brief Requests a mode set for the output
 */
void request_modeset();

/**
 * @brief Retrieves the device handle for the specified output
 * @param output Pointer to the wsm_output whose device handle is to be retrieved
 * @return Pointer to the udev_device associated with the output
 */
struct udev_device *wsm_output_get_device_handle(struct wsm_output *output);

/**
 * @brief Retrieves an output by its name or ID
 * @param name_or_id Name or ID of the output to be retrieved
 * @return Pointer to the corresponding wsm_output instance
 */
struct wsm_output *output_by_name_or_id(const char *name_or_id);

/**
 * @brief Executes a function for each workspace in the specified output
 * @param output Pointer to the wsm_output containing the workspaces
 * @param f Function to execute for each workspace
 * @param data Pointer to user data to be passed to the function
 */
void output_for_each_workspace(struct wsm_output *output,
	void (*f)(struct wsm_workspace *ws, void *data), void *data);

#endif
