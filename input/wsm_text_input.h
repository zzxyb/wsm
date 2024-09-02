/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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

struct wsm_text_input {
	struct wl_listener pending_focused_surface_destroy;

	struct wl_listener text_input_enable;
	struct wl_listener text_input_commit;
	struct wl_listener text_input_disable;
	struct wl_listener text_input_destroy;

	struct wl_list link;

	struct wsm_input_method_relay *relay;

	struct wlr_text_input_v3 *input;
	struct wlr_surface *pending_focused_surface;
};

void wsm_input_method_relay_init(struct wsm_seat *seat,
	struct wsm_input_method_relay *relay);
void wsm_input_method_relay_finish(struct wsm_input_method_relay *relay);
void wsm_input_method_relay_set_focus(struct wsm_input_method_relay *relay,
	struct wlr_surface *surface);
struct wsm_text_input *wsm_text_input_create(
	struct wsm_input_method_relay *relay,
	struct wlr_text_input_v3 *text_input);

#endif
