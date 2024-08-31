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
	struct wlr_idle_inhibit_manager_v1 *wlr_manager;
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;
};

struct wsm_idle_inhibitor_v1 {
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor;
	struct wsm_view *view;
	enum wsm_idle_inhibit_mode mode;
	
	struct wl_list link;
	struct wl_listener destroy;
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
