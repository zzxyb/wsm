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

#include "wsm_titlebar.h"
#include "wsm_log.h"
#include "node/wsm_text_node.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_titlebar* wsm_titlebar_create() {
    struct wsm_titlebar *titlebar = calloc(1, sizeof(struct wsm_titlebar));
    if (!wsm_assert(titlebar, "Could not create wsm_titlebar: allocation failed!")) {
        return NULL;
    }

    return titlebar;
}

void wsm_titlebar_destroy(struct wsm_titlebar *titlebar) {
    wlr_scene_node_destroy(&titlebar->tree->node);
    free(titlebar);
}
