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

#ifndef WSM_SWITCH_H
#define WSM_SWITCH_H

#include <wlr/types/wlr_switch.h>

struct wsm_seat;
struct wsm_seat_device;

struct wsm_switch {
	struct wl_listener switch_toggle;

	struct wsm_seat_device *seat_device;
	struct wlr_switch *wlr_switch;
	enum wlr_switch_state state;
	enum wlr_switch_type type;
};

struct wsm_switch *wsm_switch_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);
void wsm_switch_configure(struct wsm_switch *wsm_switch);
void wsm_switch_destroy(struct wsm_switch *wsm_switch);

#endif
