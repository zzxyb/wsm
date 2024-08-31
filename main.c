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

#include "wsm_log.h"
#include "config.h"
#include "common/wsm_common.h"
#include "compositor/wsm_server.h"
#include "common/wsm_parser.h"
#include "xwl/wsm_xwayland.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#include <wlr/util/log.h>
#include <wlr/backend.h>

struct wsm_server global_server = {0};

/**
 * @brief usage printf help information,understand the command options of wsm
 * @param error_code
 */
static void usage(int error_code) {
	FILE *out = error_code == EXIT_SUCCESS ? stdout : stderr;
	
	fprintf(out,
			"Usage: wsm [OPTIONS]\n\n"
			"This is wsm version " VERSION ", the Lychee's Wayland compositor.\n"
			"different options will be accepted.\n\n"
			"Core options:\n\n"
#if HAVE_XWAYLAND
			"  --xwayland\t\tLoad the xwayland module\n"
#endif
			"  -l, --log-level\tSet log output level, default value is 1 only ERROR logs, 3 output all logs, 0 disabled log\n"
			"  -h, --help\t\tThis help message\n\n");
	exit(error_code);
}

static char *copy_command_line(int argc, char * const argv[]) {
	FILE *fp;
	char *str = NULL;
	size_t size = 0;
	int i;
	
	fp = open_memstream(&str, &size);
	if (!fp) {
		return NULL;
	}
	
	fprintf(fp, "%s", argv[0]);
	for (i = 1; i < argc; i++)
		fprintf(fp, " %s", argv[i]);
	fclose(fp);
	
	return str;
}

void wsm_terminate(int exit_code) {
	if (!global_server.wl_display) {
		exit(exit_code);
	} else {
		wl_display_terminate(global_server.wl_display);
	}
}

void sig_handler(int signal) {
	wsm_log(WSM_INFO, "sig_handler: %d", signal);
	wsm_terminate(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
	char *startup_cmd = NULL;
	char *cmdline = NULL;
	bool xwayland = false;
	bool help = 0;
	int32_t log_level = WSM_ERROR;
	
	const struct wsm_option core_options[] = {
#if HAVE_XWAYLAND
											  { WSM_OPTION_BOOLEAN, "xwayland", 0, &xwayland },
#endif
											  { WSM_OPTION_INTEGER, "log-level", 'l', &log_level },
											  { WSM_OPTION_BOOLEAN, "help", 'h', &help },
											  };
	
	cmdline = copy_command_line(argc, argv);
	parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
	
	if (help) {
		free(cmdline);
		usage(EXIT_SUCCESS);
	}
	
	wsm_log(WSM_DEBUG, "Command line: %s", cmdline);
	free(cmdline);
	
	wlr_log_init(log_level, NULL);
	wsm_log_init(log_level, NULL);
	
	int c;
	while ((c = getopt(argc, argv, "s:")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			wsm_log(WSM_ERROR, "usage: %s [-s startup-command]", argv[0]);
			goto shutdown;
		}
	}
	if (optind < argc) {
		wsm_log(WSM_ERROR, "usage: %s [-s startup-command]", argv[0]);
		goto shutdown;
	}

#if HAVE_XWAYLAND
	global_server.xwayland_enabled = xwayland;
#endif
	// handle SIGTERM signals
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	
		   // prevent ipc from crashing
	signal(SIGPIPE, SIG_IGN);
	wsm_server_init(&global_server);
	
	const char *socket = wl_display_add_socket_auto(global_server.wl_display);
	if (!socket) {
		wl_display_destroy(global_server.wl_display);
		goto shutdown;
	}
	
	if (!xwayland) {
		wsm_log(WSM_DEBUG, "Command disabled xwayland!");
	}

#if HAVE_XWAYLAND
	if (xwayland ) {
		if (!xwayland_start(&global_server)) {
			wsm_log(WSM_ERROR, "xwayland start failed!");
			goto shutdown;
		}
	}
#endif
	
	if (!wlr_backend_start(global_server.backend)) {
		wl_display_destroy(global_server.wl_display);
		wsm_log(WSM_ERROR, "backend start failed!");
		goto shutdown;
	}
	
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd != NULL) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}
	
	wsm_log(WSM_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",socket);
	
	wl_display_run(global_server.wl_display);
	wl_display_destroy_clients(global_server.wl_display);
	wl_display_destroy(global_server.wl_display);
	
	return EXIT_SUCCESS;

shutdown:
	wsm_log(WSM_ERROR, "Shutting down wsm");
	server_finish(&global_server);
	
	return EXIT_FAILURE;
}
