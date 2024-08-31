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

#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_server_decoration.h"
#include "wsm_server_decoration_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_server_decoration.h>

struct wsm_server_decoration_manager *wsm_server_decoration_manager_create(const struct wsm_server* server) {
	struct wsm_server_decoration_manager *decoration_manager =
		calloc(1, sizeof(struct wsm_server_decoration_manager));
	if (!wsm_assert(decoration_manager, "Could not create wsm_server_decoration_manager: allocation failed!")) {
		return NULL;
	}

	wl_list_init(&decoration_manager->decorations);
	decoration_manager->server_decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
	wlr_server_decoration_manager_set_default_mode(decoration_manager->server_decoration_manager,
		WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);
	decoration_manager->server_decoration.notify = handle_server_decoration;
	wl_signal_add(&decoration_manager->server_decoration_manager->events.new_decoration,
		&decoration_manager->server_decoration);
	
	return decoration_manager;
}
