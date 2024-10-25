#ifndef WSM_INPUT_TEXT_INPUT_H
#define WSM_INPUT_TEXT_INPUT_H

#include "wsm_view.h"

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_input_method_v2;
struct wlr_text_input_v3;
struct wlr_text_input_manager_v3;
struct wlr_input_method_manager_v2;
struct wlr_input_popup_surface_v2;

struct wsm_seat;

struct wsm_input_method_relay {
	struct wl_listener input_method_new;
	struct wl_listener input_method_commit;
	struct wl_listener input_method_new_popup_surface;
	struct wl_listener input_method_grab_keyboard;
	struct wl_listener input_method_destroy;

	struct wl_listener input_method_keyboard_grab_destroy;

	struct wl_listener text_input_new;

	struct wl_list text_inputs; // wsm_text_input::link
	struct wl_list input_popups; // wsm_input_popup::link
	struct wlr_input_method_v2 *input_method; // doesn't have to be present

	struct wsm_seat *seat;
};

struct wsm_input_popup {
	struct wl_listener popup_destroy;
	struct wl_listener popup_surface_commit;

	struct wl_listener focused_surface_unmap;

	struct wl_list link;
	struct wsm_popup_desc desc;
	struct wsm_input_method_relay *relay;

	struct wlr_scene_tree *scene_tree;
	struct wlr_input_popup_surface_v2 *popup_surface;
};

struct wsm_text_input_v3 {
	struct wl_listener pending_focused_surface_destroy;

	struct wl_listener text_input_enable;
	struct wl_listener text_input_commit;
	struct wl_listener text_input_disable;
	struct wl_listener text_input_destroy;

	struct wl_list link;

	struct wsm_input_method_relay *relay;

	struct wlr_text_input_v3 *text_input_v3_wlr;
	struct wlr_surface *pending_focused_surface;
};

void wsm_input_method_relay_init(struct wsm_seat *seat,
	struct wsm_input_method_relay *relay);
void wsm_input_method_relay_finish(struct wsm_input_method_relay *relay);
void wsm_input_method_relay_set_focus(struct wsm_input_method_relay *relay,
	struct wlr_surface *surface);
struct wsm_text_input_v3 *wsm_text_input_v3_create(
	struct wsm_input_method_relay *relay,
	struct wlr_text_input_v3 *text_input);

#endif
