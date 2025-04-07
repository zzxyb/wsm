#ifndef WSM_VIEW_H
#define WSM_VIEW_H

#include "../config.h"

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>

struct wlr_scene_node;
struct wlr_scene_tree;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
#if HAVE_XWAYLAND
struct wlr_xwayland_surface;
#endif
struct wlr_foreign_toplevel_handle_v1;
struct wlr_ext_foreign_toplevel_handle_v1;

struct wsm_seat;
struct wsm_view;
struct wsm_output;
struct wsm_container;
struct wsm_workspace;
struct wsm_xdg_decoration;

/**
 * @brief Enumeration of view types in the WSM
 */
enum wsm_view_type {
	WSM_VIEW_XDG_SHELL, /**< XDG shell view type */
#if HAVE_XWAYLAND
	WSM_VIEW_XWAYLAND, /**< XWayland view type */
#endif
};

/**
 * @brief Enumeration of properties for views
 */
enum wsm_view_prop {
	VIEW_PROP_TITLE, /**< Title property */
	VIEW_PROP_APP_ID, /**< Application ID property */
	VIEW_PROP_CLASS, /**< Class property */
	VIEW_PROP_INSTANCE, /**< Instance property */
	VIEW_PROP_WINDOW_TYPE, /**< Window type property */
	VIEW_PROP_WINDOW_ROLE, /**< Window role property */
#if HAVE_XWAYLAND
	VIEW_PROP_X11_WINDOW_ID, /**< X11 window ID property */
	VIEW_PROP_X11_PARENT_ID, /**< X11 parent ID property */
#endif
};

/**
 * @brief Structure representing the implementation of a view
 */
struct wsm_view_impl {
	void (*get_constraints)(struct wsm_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height); /**< Function to get size constraints */
	const char *(*get_string_prop)(struct wsm_view *view,
		enum wsm_view_prop prop); /**< Function to get string properties */
	uint32_t (*get_int_prop)(struct wsm_view *view, enum wsm_view_prop prop); /**< Function to get integer properties */
	uint32_t (*configure)(struct wsm_view *view, double lx, double ly,
		int width, int height); /**< Function to configure the view */
	void (*set_activated)(struct wsm_view *view, bool activated); /**< Function to set activation state */
	void (*set_tiled)(struct wsm_view *view, bool tiled); /**< Function to set tiled state */
	void (*set_fullscreen)(struct wsm_view *view, bool fullscreen); /**< Function to set fullscreen state */
	void (*set_resizing)(struct wsm_view *view, bool resizing); /**< Function to set resizing state */
	bool (*wants_floating)(struct wsm_view *view); /**< Function to check if the view wants to float */
	bool (*is_transient_for)(struct wsm_view *child,
		struct wsm_view *ancestor); /**< Function to check transient relationship */
	void (*maximize)(struct wsm_view *view, bool maximize); /**< Function to maximize the view */
	void (*minimize)(struct wsm_view *view, bool minimize); /**< Function to minimize the view */
	void (*close)(struct wsm_view *view); /**< Function to close the view */
	void (*close_popups)(struct wsm_view *view); /**< Function to close popups associated with the view */
	void (*destroy)(struct wsm_view *view); /**< Function to destroy the view */
};

/**
 * @brief Structure representing a view in the WSM
 */
struct wsm_view {
	struct wl_listener foreign_activate_request; /**< Listener for foreign activate requests */
	struct wl_listener foreign_fullscreen_request; /**< Listener for foreign fullscreen requests */
	struct wl_listener foreign_close_request; /**< Listener for foreign close requests */
	struct wl_listener foreign_destroy; /**< Listener for foreign destroy events */

	struct {
		struct wl_signal unmap; /**< Signal for unmap events */
	} events; /**< Events associated with the view */

	struct wlr_box geometry; /**< Geometry of the view */
	struct timespec urgent; /**< Urgent state timestamp */

	const struct wsm_view_impl *impl; /**< Pointer to the view implementation */

	struct wlr_scene_tree *scene_tree; /**< Scene tree for the view */
	struct wlr_scene_tree *content_tree; /**< Content scene tree for the view */
	struct wlr_scene_tree *saved_surface_tree; /**< Saved surface tree for the view */

	struct wsm_container *container; /**< Pointer to the associated container (NULL if unmapped) */
	struct wlr_surface *surface; /**< Pointer to the associated WLR surface (NULL for unmapped views) */
	struct wsm_xdg_decoration *xdg_decoration; /**< Pointer to the XDG decoration */

	char *title_format; /**< Format string for the title */
	char *app_id; /**< Application ID for the view */
	char *app_icon_path; /**< Path to the application icon */

	struct wl_event_source *urgent_timer; /**< Timer for urgent state */
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel; /**< Pointer to the external foreign toplevel handle */
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel; /**< Pointer to the foreign toplevel handle */

	union {
		struct wlr_xdg_toplevel *wlr_xdg_toplevel; /**< Pointer to the WLR XDG toplevel */
#if HAVE_XWAYLAND
		struct wlr_xwayland_surface *wlr_xwayland_surface; /**< Pointer to the WLR XWayland surface */
#endif
	};
	pid_t pid; /**< Process ID of the view */
	enum wsm_view_type type; /**< Type of the view */
	int natural_width, natural_height; /**< Natural dimensions of the view */
	int max_render_time; /**< Maximum render time in milliseconds */
	bool using_csd; /**< Flag indicating if client-side decorations are used */
	bool enabled; /**< Flag indicating if the view is enabled */
	bool destroying; /**< Flag indicating if the view is being destroyed */
	bool allow_request_urgent; /**< Flag indicating if urgent requests are allowed */
};

/**
 * @brief Structure representing an XDG shell view
 */
struct wsm_xdg_shell_view {
	struct wsm_view view; /**< Base view structure */

	struct wl_listener commit; /**< Listener for commit events */
	struct wl_listener request_move; /**< Listener for move requests */
	struct wl_listener request_resize; /**< Listener for resize requests */
	struct wl_listener request_maximize; /**< Listener for maximize requests */
	struct wl_listener request_fullscreen; /**< Listener for fullscreen requests */
	struct wl_listener set_title; /**< Listener for title set events */
	struct wl_listener set_app_id; /**< Listener for application ID set events */
	struct wl_listener new_popup; /**< Listener for new popup events */
	struct wl_listener map; /**< Listener for map events */
	struct wl_listener unmap; /**< Listener for unmap events */
	struct wl_listener destroy; /**< Listener for destroy events */
};

#if HAVE_XWAYLAND
/**
 * @brief Structure representing a XWayland view
 */
struct wsm_xwayland_view {
	struct wsm_view view; /**< Base view structure */

	struct wl_listener commit; /**< Listener for commit events */
	struct wl_listener request_move; /**< Listener for move requests */
	struct wl_listener request_resize; /**< Listener for resize requests */
	struct wl_listener request_maximize; /**< Listener for maximize requests */
	struct wl_listener request_minimize; /**< Listener for minimize requests */
	struct wl_listener request_configure; /**< Listener for configure requests */
	struct wl_listener request_fullscreen; /**< Listener for fullscreen requests */
	struct wl_listener request_activate; /**< Listener for activate requests */
	struct wl_listener set_title; /**< Listener for title set events */
	struct wl_listener set_class; /**< Listener for class set events */
	struct wl_listener set_role; /**< Listener for role set events */
	struct wl_listener set_startup_id; /**< Listener for startup ID set events */
	struct wl_listener set_window_type; /**< Listener for window type set events */
	struct wl_listener set_hints; /**< Listener for hints set events */
	struct wl_listener set_decorations; /**< Listener for decorations set events */
	struct wl_listener associate; /**< Listener for associate events */
	struct wl_listener dissociate; /**< Listener for dissociate events */
	struct wl_listener map; /**< Listener for map events */
	struct wl_listener unmap; /**< Listener for unmap events */
	struct wl_listener destroy; /**< Listener for destroy events */
	struct wl_listener override_redirect; /**< Listener for override redirect events */

	struct wl_listener surface_tree_destroy; /**< Listener for surface tree destroy events */

	struct wlr_scene_tree *surface_tree; /**< Scene tree for the surface */
};

#endif

/**
 * @brief Structure representing a popup description
 */
struct wsm_popup_desc {
	struct wlr_scene_node *relative; /**< Pointer to the relative scene node */
	struct wsm_view *view; /**< Pointer to the associated view */
};

/**
 * @brief Initializes a view
 * @param view Pointer to the wsm_view instance to initialize
 * @param type Type of the view
 * @param impl Pointer to the view implementation
 * @return true if initialization was successful, false otherwise
 */
bool view_init(struct wsm_view *view, enum wsm_view_type type,
	const struct wsm_view_impl *impl);

/**
 * @brief Destroys a view
 * @param view Pointer to the wsm_view instance to destroy
 */
void view_destroy(struct wsm_view *view);

/**
 * @brief Begins the destruction process for a view
 * @param view Pointer to the wsm_view instance to begin destruction for
 */
void view_begin_destroy(struct wsm_view *view);

/**
 * @brief Retrieves the title of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the title string
 */
const char *view_get_title(struct wsm_view *view);

/**
 * @brief Retrieves the application ID of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the application ID string
 */
const char *view_get_app_id(struct wsm_view *view);

/**
 * @brief Retrieves the class of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the class string
 */
const char *view_get_class(struct wsm_view *view);

/**
 * @brief Retrieves the instance of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the instance string
 */
const char *view_get_instance(struct wsm_view *view);

/**
 * @brief Retrieves the X11 window ID of a view
 * @param view Pointer to the wsm_view instance
 * @return X11 window ID
 */
uint32_t view_get_x11_window_id(struct wsm_view *view);

/**
 * @brief Retrieves the X11 parent ID of a view
 * @param view Pointer to the wsm_view instance
 * @return X11 parent ID
 */
uint32_t view_get_x11_parent_id(struct wsm_view *view);

/**
 * @brief Retrieves the window role of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the window role string
 */
const char *view_get_window_role(struct wsm_view *view);

/**
 * @brief Retrieves the window type of a view
 * @param view Pointer to the wsm_view instance
 * @return Window type
 */
uint32_t view_get_window_type(struct wsm_view *view);

/**
 * @brief Retrieves the shell type of a view
 * @param view Pointer to the wsm_view instance
 * @return Pointer to the shell type string
 */
const char *view_get_shell(struct wsm_view *view);

/**
 * @brief Gets the size constraints for a view
 * @param view Pointer to the wsm_view instance
 * @param min_width Pointer to store the minimum width
 * @param max_width Pointer to store the maximum width
 * @param min_height Pointer to store the minimum height
 * @param max_height Pointer to store the maximum height
 */
void view_get_constraints(struct wsm_view *view, double *min_width,
	double *max_width, double *min_height, double *max_height);

/**
 * @brief Configures a view with new dimensions
 * @param view Pointer to the wsm_view instance
 * @param lx New X position
 * @param ly New Y position
 * @param width New width
 * @param height New height
 * @return Configuration result
 */
uint32_t view_configure(struct wsm_view *view, double lx, double ly, int width,
	int height);

/**
 * @brief Inhibits idle state for a view
 * @param view Pointer to the wsm_view instance
 * @return true if idle is inhibited, false otherwise
 */
bool view_inhibit_idle(struct wsm_view *view);

/**
 * @brief Automatically configures a view
 * @param view Pointer to the wsm_view instance
 */
void view_autoconfigure(struct wsm_view *view);

/**
 * @brief Sets the activation state for a view
 * @param view Pointer to the wsm_view instance
 * @param activated Flag indicating if the view is activated
 */
void view_set_activated(struct wsm_view *view, bool activated);

/**
 * @brief Requests activation for a view
 * @param view Pointer to the wsm_view instance
 * @param seat Pointer to the associated wsm_seat instance
 */
void view_request_activate(struct wsm_view *view, struct wsm_seat *seat);

/**
 * @brief Requests urgent state for a view
 * @param view Pointer to the wsm_view instance
 */
void view_request_urgent(struct wsm_view *view);

/**
 * @brief Sets client-side decorations from the server for a view
 * @param view Pointer to the wsm_view instance
 * @param enabled Flag indicating if CSD is enabled
 */
void view_set_csd_from_server(struct wsm_view *view, bool enabled);

/**
 * @brief Updates client-side decorations from the client for a view
 * @param view Pointer to the wsm_view instance
 * @param enabled Flag indicating if CSD is enabled
 */
void view_update_csd_from_client(struct wsm_view *view, bool enabled);

/**
 * @brief Sets the tiled state for a view
 * @param view Pointer to the wsm_view instance
 * @param tiled Flag indicating if the view is tiled
 */
void view_set_tiled(struct wsm_view *view, bool tiled);

/**
 * @brief Maximizes a view
 * @param view Pointer to the wsm_view instance
 * @param maximize Flag indicating if the view should be maximized
 */
void view_maximize(struct wsm_view *view, bool maximize);

/**
 * @brief Minimizes a view
 * @param view Pointer to the wsm_view instance
 * @param minimize Flag indicating if the view should be minimized
 */
void view_minimize(struct wsm_view *view, bool minimize);

/**
 * @brief Closes a view
 * @param view Pointer to the wsm_view instance
 */
void view_close(struct wsm_view *view);

/**
 * @brief Closes popups associated with a view
 * @param view Pointer to the wsm_view instance
 */
void view_close_popups(struct wsm_view *view);

/**
 * @brief Enables or disables a view
 * @param view Pointer to the wsm_view instance
 * @param enable Flag indicating if the view should be enabled
 */
void view_set_enable(struct wsm_view *view, bool enable);

/**
 * @brief Maps a view to a surface
 * @param view Pointer to the wsm_view instance
 * @param wlr_surface Pointer to the WLR surface to map
 * @param fullscreen Flag indicating if the view should be fullscreen
 * @param fullscreen_output Pointer to the output for fullscreen
 * @param decoration Flag indicating if decorations should be applied
 */
void view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
	bool fullscreen, struct wlr_output *fullscreen_output, bool decoration);

/**
 * @brief Unmaps a view
 * @param view Pointer to the wsm_view instance
 */
void view_unmap(struct wsm_view *view);

/**
 * @brief Updates the size of a view
 * @param view Pointer to the wsm_view instance
 */
void view_update_size(struct wsm_view *view);

/**
 * @brief Centers and clips the surface of a view
 * @param view Pointer to the wsm_view instance
 */
void view_center_and_clip_surface(struct wsm_view *view);

/**
 * @brief Retrieves the view associated with a WLR surface
 * @param surface Pointer to the WLR surface
 * @return Pointer to the associated wsm_view instance
 */
struct wsm_view *view_from_wlr_surface(struct wlr_surface *surface);

/**
 * @brief Updates the application ID for a view
 * @param view Pointer to the wsm_view instance
 */
void view_update_app_id(struct wsm_view *view);

/**
 * @brief Updates the title for a view
 * @param view Pointer to the wsm_view instance
 * @param force Flag indicating if the update should be forced
 */
void view_update_title(struct wsm_view *view, bool force);

/**
 * @brief Checks if a view is visible
 * @param view Pointer to the wsm_view instance
 * @return true if the view is visible, false otherwise
 */
bool view_is_visible(struct wsm_view *view);

/**
 * @brief Sets the urgent state for a view
 * @param view Pointer to the wsm_view instance
 * @param enable Flag indicating if the view should be urgent
 */
void view_set_urgent(struct wsm_view *view, bool enable);

/**
 * @brief Checks if a view is in an urgent state
 * @param view Pointer to the wsm_view instance
 * @return true if the view is urgent, false otherwise
 */
bool view_is_urgent(struct wsm_view *view);

/**
 * @brief Removes the saved buffer for a view
 * @param view Pointer to the wsm_view instance
 */
void view_remove_saved_buffer(struct wsm_view *view);

/**
 * @brief Saves the buffer for a view
 * @param view Pointer to the wsm_view instance
 */
void view_save_buffer(struct wsm_view *view);

/**
 * @brief Checks if a view is transient for another view
 * @param child Pointer to the child wsm_view instance
 * @param ancestor Pointer to the ancestor wsm_view instance
 * @return true if the child is transient for the ancestor, false otherwise
 */
bool view_is_transient_for(struct wsm_view *child, struct wsm_view *ancestor);

/**
 * @brief Sends a frame done signal for a view
 * @param view Pointer to the wsm_view instance
 */
void view_send_frame_done(struct wsm_view *view);

#endif
