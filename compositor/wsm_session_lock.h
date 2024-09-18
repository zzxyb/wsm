#ifndef WSM_SESSION_LOCK_H
#define WSM_SESSION_LOCK_H

#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_session_lock_v1;

struct wsm_output;

struct wsm_session_lock {
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;

	struct wl_list outputs; // struct wsm_session_lock_output

	struct wlr_session_lock_v1 *lock;
	struct wlr_surface *focused;
	bool abandoned;
};

void wsm_session_lock_init(void);
void wsm_session_lock_add_output(struct wsm_session_lock *lock,
	struct wsm_output *output);
bool wsm_session_lock_has_surface(struct wsm_session_lock *lock,
	struct wlr_surface *surface);

#endif
