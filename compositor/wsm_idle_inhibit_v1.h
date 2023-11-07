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

struct wlr_idle_inhibitor_v1;
struct wlr_idle_notifier_v1;
struct wlr_idle_inhibit_manager_v1;

struct wsm_idle_inhibit_manager_v1 {
    struct wlr_idle_inhibit_manager_v1 *wlr_inhibit_manager_v1;
    struct wlr_idle_notifier_v1 *wlr_idle_notifier_v1;
    struct wl_listener new_idle_inhibit_v1;
    struct wl_list inhibits;
};

struct wsm_idle_inhibit_v1 {
    struct wlr_idle_inhibitor_v1 *wlr_inhibit_v1;

    struct wl_list link;
    struct wl_listener destroy;
};

struct wsm_idle_inhibit_manager_v1 *wsm_idle_inhibit_manager_v1_create();
bool wsm_idle_inhibit_v1_is_active(struct wsm_idle_inhibit_v1 *inhibitor);
void wsm_idle_inhibit_v1_check_active();

#endif
