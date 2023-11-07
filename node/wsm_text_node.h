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

#ifndef WSM_TEXT_NODE_H
#define WSM_TEXT_NODE_H

#include <stdbool.h>

struct wlr_scene_node;
struct wlr_scene_tree;

struct wsm_text_node {
    int width;
    int max_width;
    int height;
    int baseline;
    bool pango_markup;
    float color[4];
    float background[4];

    struct wlr_scene_node *node;
};

struct wsm_text_node *wsm_text_node_create(struct wlr_scene_tree *parent,
                                             char *text, float color[4], bool pango_markup);
void wsm_text_node_destroy(struct wsm_text_node *node);
void wsm_text_node_set_color(struct wsm_text_node *node, float color[4]);
void wsm_text_node_set_text(struct wsm_text_node *node, char *text);
void wsm_text_node_set_max_width(struct wsm_text_node *node, int max_width);
void wsm_text_node_set_background(struct wsm_text_node *node, float background[4]);

#endif
