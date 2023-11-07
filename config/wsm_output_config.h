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

#ifndef WSM_OUTPUT_CONFIG_H
#define WSM_OUTPUT_CONFIG_H

#include <stdbool.h>
#include <stdio.h>

// #include <libxml/parser.h>
// #include <libxml/tree.h>
// #include <libxml/xmlwriter.h>

struct wsm_list;
struct wsm_output;
struct wsm_output_manager;

struct wsm_output_config {
    struct wsm_output *output;
    // xmlNodePtr node;
    struct wsm_list *attachs;
};

struct wsm_output_config *wsm_output_config_create(struct wsm_output *output);
void wsm_output_config_destory(struct wsm_output_config *output);

#endif
