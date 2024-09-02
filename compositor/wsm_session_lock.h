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
