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
#include "wsm_server.h"
#include "wsm_server_decoration_manager.h"
#include "wsm_server_decoration.h"

#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_compositor.h>

struct wsm_server_decoration *decoration_from_surface(
    struct wlr_surface *surface) {
    struct wsm_server_decoration *deco;
    wl_list_for_each(deco, &server.wsm_server_decoration_manager->decorations, link) {
        if (deco->wlr_server_decoration->surface == surface) {
            return deco;
        }
    }
    return NULL;
}

void handle_server_decoration(struct wl_listener *listener, void *data) {

}
