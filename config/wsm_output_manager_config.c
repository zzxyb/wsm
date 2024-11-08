/*
 * Copyright © 2024 YaoBing Xiao
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "wsm_log.h"
#include "wsm_list.h"
#include "wsm_output_manager.h"
#include "wsm_output_manager_config.h"

#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#define DEFAULT_OUTPUT_CONFIG "/home/xyb/Documents/Code/Github/wsm/config/data/default_output_config.xml"

struct wsm_output_manager_config *wsm_output_manager_config_create(struct wsm_output_manager *output_manager) {
	struct wsm_output_manager_config *outputs_manager_config =
		calloc(1, sizeof(struct wsm_output_manager_config));
	if (!outputs_manager_config) {
		wsm_log(WSM_ERROR, "Could not create wsm_output_manager: allocation failed!");
		return NULL;
	}

	outputs_manager_config->configs = wsm_list_create();
	return outputs_manager_config;
}

void wwsm_output_manager_config_destory(struct wsm_output_manager_config *config) {
	
}
