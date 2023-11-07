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

#define _POSIX_C_SOURCE 200809L
#include "wsm_layer_shell.h"
#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_output.h"

#include <stdlib.h>

#define WSM_LAYER_SHELL_VERSION 4

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {

}

struct wsm_layer_shell *wsm_layer_shell_create() {
    struct wsm_layer_shell *layer_shell = calloc(1, sizeof(struct wsm_layer_shell));
    if (!wsm_assert(layer_shell, "Could not create wsm_layer_shell: allocation failed!")) {
        return NULL;
    }

    layer_shell->wlr_layer_shell = wlr_layer_shell_v1_create(server.wl_display,
                                                    WSM_LAYER_SHELL_VERSION);
    layer_shell->layer_shell_surface.notify = handle_layer_shell_surface;
    wl_signal_add(&layer_shell->wlr_layer_shell->events.new_surface,
                  &layer_shell->layer_shell_surface);

    return layer_shell;
}

void layers_arrange(struct wsm_output *output) {

}

void layer_try_set_focus(struct wsm_seat *seat, struct wlr_layer_surface_v1 *layer_surface) {

}
