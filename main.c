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

struct wsm_server server = {0};

/**
 * @brief registerDbusComponent call dbus to notify other components that wsm is ready,
 * now wayland client can start connecting
 */
void registerDbusComponent() {
    int ret = 0;
    sd_bus *bus = NULL;
    ret = sd_bus_open_user(&bus);
    if (ret < 0) {
        wsm_log(WSM_ERROR, "Failed to connect to system bus: %s", strerror(-ret));
        return;
    }

    sd_bus_message *msg = NULL;
    ret = sd_bus_message_new_method_call(bus, &msg,
                                         "org.lychee.Startup",
                                         "/Startup",
                                         "org.lychee.Startup",
                                         "registerComponent");
    if (ret < 0) {
        wsm_log(WSM_ERROR, "Failed to create D-Bus message: %s", strerror(-ret));
        sd_bus_unref(bus);
        return;
    }

    const char *param = "wsm";
    ret = sd_bus_message_append(msg, "s", param);
    if (ret < 0) {
        wsm_log(WSM_ERROR, "Failed to append string parameter: %s", strerror(-ret));
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return;
    }

    sd_bus_message *reply = NULL;
    ret = sd_bus_call(bus, msg, 0, NULL, &reply);
    if (ret < 0) {
        wsm_log(WSM_ERROR, "Failed to call D-Bus method: %s", strerror(-ret));
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return;
    }

    if (ret >= 0)
        wsm_log(WSM_INFO, "registerComponent successfully");

    // Clean up resources
    sd_bus_message_unref(reply);
    sd_bus_message_unref(msg);
    sd_bus_unref(bus);
}

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
#ifdef HAVE_XWAYLAND
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
    if (!fp)
        return NULL;

    fprintf(fp, "%s", argv[0]);
    for (i = 1; i < argc; i++)
        fprintf(fp, " %s", argv[i]);
    fclose(fp);

    return str;
}

int main(int argc, char **argv) {
    char *startup_cmd = NULL;
    char *cmdline = NULL;
    bool xwayland = false;
    bool help = 0;
    int32_t log_level = WSM_ERROR;

    const struct wsm_option core_options[] = {
#ifdef HAVE_XWAYLAND
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

    wsm_server_init(&server);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        wl_display_destroy(server.wl_display);
        goto shutdown;
    }

    if (!xwayland) {
        wsm_log(WSM_DEBUG, "Command disabled xwayland!");
    }

#ifdef HAVE_XWAYLAND
    if (xwayland ) {
        if (!xwayland_start(&server)) {
            wsm_log(WSM_ERROR, "xwayland start failed!");
            goto shutdown;
        }
    }
#endif

    if (!wlr_backend_start(server.backend)) {
        wl_display_destroy(server.wl_display);
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

    registerDbusComponent();

    wl_display_run(server.wl_display);
    wl_display_destroy_clients(server.wl_display);
    wl_display_destroy(server.wl_display);
    return EXIT_SUCCESS;

shutdown:
    wsm_log(WSM_ERROR, "Shutting down wsm");
    if (server.wl_display)
        wl_display_terminate(server.wl_display);

    return EXIT_FAILURE;
}
