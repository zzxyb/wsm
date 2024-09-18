/** \file
 *
 *  \brief premits client to inhibit the idle behaviror such as
 *  screenblanking, locking and screensaving.
 *
 *  \warning Implemented based on wlr_idle_inhibit.
 */

#ifndef WSM_IDLE_INHIBIT_V1_H
#define WSM_IDLE_INHIBIT_V1_H

#include <wayland-server-core.h>

struct wlr_idle_inhibit_manager_v1;
struct wlr_idle_inhibitor_v1;

struct wsm_view;

enum wsm_idle_inhibit_mode {
	INHIBIT_IDLE_APPLICATION,  // Application set inhibitor (when visible)
	INHIBIT_IDLE_FOCUS,  // User set inhibitor when focused
	INHIBIT_IDLE_FULLSCREEN,  // User set inhibitor when fullscreen + visible
	INHIBIT_IDLE_OPEN,  // User set inhibitor while open
	INHIBIT_IDLE_VISIBLE  // User set inhibitor when visible
};

struct wsm_idle_inhibit_manager_v1 {
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;
	struct wlr_idle_inhibit_manager_v1 *wlr_manager;
};

struct wsm_idle_inhibitor_v1 {
	struct wl_listener destroy;
	struct wl_list link;
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor;
	struct wsm_view *view;
	enum wsm_idle_inhibit_mode mode;
};

bool wsm_idle_inhibit_v1_is_active(
		struct wsm_idle_inhibitor_v1 *inhibitor);
void wsm_idle_inhibit_v1_check_active(void);
void wsm_idle_inhibit_v1_user_inhibitor_register(struct wsm_view *view,
	enum wsm_idle_inhibit_mode mode);
struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_user_inhibitor_for_view(
	struct wsm_view *view);
struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_application_inhibitor_for_view(
	struct wsm_view *view);
void wsm_idle_inhibit_v1_user_inhibitor_destroy(
	struct wsm_idle_inhibitor_v1 *inhibitor);
bool wsm_idle_inhibit_manager_v1_init(void);

#endif
