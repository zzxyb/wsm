/** \file
 *
 *  \brief Service core, manages global resources and initialization.
 *
 */

#ifndef WSM_SERVER_H
#define WSM_SERVER_H

#include "wsm_idle_inhibit_v1.h"

#include "../config.h"
#ifdef HAVE_XWAYLAND
#include "wsm_xwayland.h"
#endif
#include <wayland-server-core.h>

struct wlr_idle;
struct wl_display;
struct wsm_output;
struct wlr_backend;
struct wlr_surface;
struct wlr_session;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;
struct wlr_presentation;
struct wlr_output_layout;
struct wlr_session_lock_v1;
struct wlr_linux_dmabuf_v1;
struct wlr_xcursor_manager;
struct wlr_idle_notifier_v1;
struct wlr_output_manager_v1;
struct wlr_data_device_manager;
struct wlr_drm_lease_v1_manager;
struct wlr_pointer_constraint_v1;
struct wlr_security_context_manager_v1;
struct wlr_xdg_activation_v1;
struct wlr_content_type_manager_v1;
struct wlr_data_control_manager_v1;
struct wlr_screencopy_manager_v1;
struct wlr_pointer_constraints_v1;
struct wlr_export_dmabuf_manager_v1;
struct wlr_session_lock_manager_v1;
struct wlr_relative_pointer_manager_v1;
struct wlr_foreign_toplevel_manager_v1;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_input_method_manager_v2;
struct wlr_text_input_manager_v3;

struct wsm_font;
struct wsm_list;
struct wsm_xdg_shell;
struct wsm_layer_shell;
struct wsm_scene;
struct wsm_cursor;
struct wsm_transaction;
struct wsm_session_lock;
struct wsm_input_manager;
struct wsm_output_manager;
struct wsm_desktop_interface;
struct wsm_xdg_decoration_manager;
struct wsm_server_decoration_manager;

/**
 * @brief server global server object
 */
extern struct wsm_server global_server;

/**
 * @brief Structure representing a pointer constraint in the WSM
 */
struct wsm_pointer_constraint {
	struct wl_listener set_region; /**< Listener for setting the constraint region */
	struct wl_listener destroy; /**< Listener for destruction events */

	struct wsm_cursor *cursor; /**< Pointer to the associated cursor */
	struct wlr_pointer_constraint_v1 *constraint; /**< Pointer to the WLR pointer constraint instance */
};

/**
 * @brief Structure representing the WSM server
 */
struct wsm_server {
#ifdef HAVE_XWAYLAND
	struct wsm_xwayland xwayland; /**< XWayland instance */
	struct wl_listener xwayland_surface; /**< Listener for XWayland surface events */
	struct wl_listener xwayland_ready; /**< Listener for XWayland readiness events */
#endif

	struct wl_listener pointer_constraint; /**< Listener for pointer constraint events */
	struct wl_listener drm_lease_request; /**< Listener for DRM lease requests */

	struct {
		struct wl_listener new_lock; /**< Listener for new session lock events */
		struct wl_listener manager_destroy; /**< Listener for session lock manager destruction events */

		struct wsm_session_lock *lock; /**< Pointer to the session lock */
		struct wlr_session_lock_manager_v1 *manager; /**< Pointer to the WLR session lock manager */
	} session_lock; /**< Session lock management */

	const char *socket; /**< Socket for the server */
	struct wl_display *wl_display; /**< Pointer to the Wayland display */
	struct wl_event_loop *wl_event_loop; /**< Pointer to the Wayland event loop */
	struct wlr_backend *backend; /**< Pointer to the WLR backend */
	struct wlr_session *wlr_session; /**< Pointer to the WLR session */
	struct wlr_backend *headless_backend; /**< Pointer to the headless WLR backend */
	struct wlr_renderer *wlr_renderer; /**< Pointer to the WLR renderer */
	struct wlr_allocator *wlr_allocator; /**< Pointer to the WLR allocator */
	struct wlr_compositor *wlr_compositor; /**< Pointer to the WLR compositor */
	struct wlr_relative_pointer_manager_v1 *wlr_relative_pointer_manager; /**< Pointer to the relative pointer manager */

	struct wlr_data_device_manager *data_device_manager; /**< Pointer to the data device manager */
	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1; /**< Pointer to the Linux DMA-BUF manager */
	struct wlr_security_context_manager_v1 *security_context_manager_v1; /**< Pointer to the security context manager */
	struct wlr_idle_notifier_v1 *idle_notifier_v1; /**< Pointer to the idle notifier */
	struct wlr_presentation *presentation; /**< Pointer to the presentation manager */
	struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list; /**< Pointer to the foreign toplevel list */
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager; /**< Pointer to the foreign toplevel manager */
	struct wlr_drm_lease_v1_manager *drm_lease_manager; /**< Pointer to the DRM lease manager */
	struct wlr_content_type_manager_v1 *content_type_manager_v1; /**< Pointer to the content type manager */
	struct wlr_data_control_manager_v1 *data_control_manager_v1; /**< Pointer to the data control manager */
	struct wlr_screencopy_manager_v1 *screencopy_manager_v1; /**< Pointer to the screencopy manager */
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager_v1; /**< Pointer to the export DMA-BUF manager */
	struct wlr_xdg_activation_v1 *xdg_activation_v1; /**< Pointer to the XDG activation manager */
	struct wlr_pointer_constraints_v1 *pointer_constraints; /**< Pointer to the pointer constraints manager */
	struct wlr_input_method_manager_v2 *input_method; /**< Pointer to the input method manager */
	struct wlr_text_input_manager_v3 *text_input; /**< Pointer to the text input manager */

#ifdef HAVE_XWAYLAND
	struct wlr_xcursor_manager *xcursor_manager; /**< Pointer to the X cursor manager */
#endif

	struct wsm_scene *scene; /**< Pointer to the scene manager */
	struct wsm_xdg_shell *xdg_shell; /**< Pointer to the XDG shell manager */
	struct wsm_layer_shell *layer_shell; /**< Pointer to the layer shell manager */
	struct wsm_input_manager *input_manager; /**< Pointer to the input manager */
	struct wsm_output_manager *output_manager; /**< Pointer to the output manager */
	struct wsm_server_decoration_manager *server_decoration_manager; /**< Pointer to the server decoration manager */
	struct wsm_xdg_decoration_manager *xdg_decoration_manager; /**< Pointer to the XDG decoration manager */
	struct wsm_idle_inhibit_manager_v1 idle_inhibit_manager_v1; /**< Idle inhibit manager instance */
	struct wsm_desktop_interface *desktop_interface; /**< Pointer to the desktop interface */

	size_t txn_timeout_ms; /**< Transaction timeout in milliseconds */
	struct wsm_transaction *queued_transaction; /**< Pointer to the queued transaction */
	struct wsm_transaction *pending_transaction; /**< Pointer to the pending transaction */
	struct wsm_list *dirty_nodes; /**< List of dirty nodes */
	struct wl_event_source *delayed_modeset; /**< Pointer to the delayed modeset event source */
	bool xwayland_enabled; /**< Flag indicating if XWayland is enabled */
};

/**
 * @brief Initializes the WSM server
 * @param server Pointer to the wsm_server instance to initialize
 * @return true if initialization was successful, false otherwise
 */
bool wsm_server_init(struct wsm_server *server);

/**
 * @brief Finalizes the WSM server, cleaning up resources
 * @param server Pointer to the wsm_server instance to finalize
 */
void server_finish(struct wsm_server *server);

#endif
