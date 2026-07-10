#include "wsm-brightness-management-unstable-v1-client-protocol.h"

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

struct client_state;

struct output_state {
	struct client_state *client;
	struct output_state *next;
	struct wl_output *output;
	struct wsm_brightness_control_v1 *brightness_control;
	char *name;
	uint32_t brightness;
	uint32_t min_brightness;
	uint32_t max_brightness;
	uint32_t method;
	bool initialized;
	bool failed;
};

struct client_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wsm_brightness_control_manager_v1 *manager;
	struct output_state *outputs;
	const char *requested_output;
	uint32_t requested_brightness;
	int pending_initializations;
	bool brightness_requested;
	bool target_found;
	bool set_sent;
	bool set_failed;
	bool local_error;
};

static void output_geometry(void *data, struct wl_output *output, int32_t x,
		int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
}

static void output_done(void *data, struct wl_output *output) {
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
}

static void output_name(void *data, struct wl_output *output,
		const char *name) {
	struct output_state *state = data;
	char *copy = strdup(name);
	if (!copy) {
		state->client->local_error = true;
		return;
	}
	free(state->name);
	state->name = copy;
}

static void output_description(void *data, struct wl_output *output,
		const char *description) {
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
	.name = output_name,
	.description = output_description,
};

static void registry_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct client_state *client = data;
	if (strcmp(interface,
			wsm_brightness_control_manager_v1_interface.name) == 0) {
		client->manager = wl_registry_bind(registry, name,
			&wsm_brightness_control_manager_v1_interface,
			version < 1 ? version : 1);
		return;
	}
	if (strcmp(interface, wl_output_interface.name) != 0) {
		return;
	}

	struct output_state *output = calloc(1, sizeof(*output));
	if (!output) {
		client->local_error = true;
		return;
	}
	output->client = client;
	uint32_t bind_version = version < 4 ? version : 4;
	output->output = wl_registry_bind(registry, name,
		&wl_output_interface, bind_version);
	if (!output->output || wl_output_add_listener(output->output,
			&output_listener, output) < 0) {
		free(output);
		client->local_error = true;
		return;
	}
	output->next = client->outputs;
	client->outputs = output;
}

static void registry_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static const char *method_name(uint32_t method) {
	switch (method) {
	case WSM_BRIGHTNESS_CONTROL_V1_METHOD_NONE:
		return "none";
	case WSM_BRIGHTNESS_CONTROL_V1_METHOD_BACKLIGHT:
		return "backlight";
	case WSM_BRIGHTNESS_CONTROL_V1_METHOD_DDCUTIL:
		return "ddcutil";
	default:
		return "unknown";
	}
}

static void brightness(void *data,
		struct wsm_brightness_control_v1 *control, uint32_t value) {
	struct output_state *output = data;
	output->brightness = value;
}

static void min_brightness(void *data,
		struct wsm_brightness_control_v1 *control, uint32_t value) {
	struct output_state *output = data;
	output->min_brightness = value;
}

static void max_brightness(void *data,
		struct wsm_brightness_control_v1 *control, uint32_t value) {
	struct output_state *output = data;
	output->max_brightness = value;
}

static void brightness_method(void *data,
		struct wsm_brightness_control_v1 *control, uint32_t method) {
	struct output_state *output = data;
	output->method = method;
}

static void brightness_done(void *data,
		struct wsm_brightness_control_v1 *control) {
	struct output_state *output = data;
	struct client_state *client = output->client;
	if (output->initialized || output->failed) {
		return;
	}
	output->initialized = true;
	client->pending_initializations--;
	printf("output=%s method=%s brightness=%u min_brightness=%u "
		"max_brightness=%u\n", output->name, method_name(output->method),
		output->brightness, output->min_brightness, output->max_brightness);

	if (!client->brightness_requested) {
		return;
	}
	if (output->method == WSM_BRIGHTNESS_CONTROL_V1_METHOD_NONE) {
		fprintf(stderr, "output %s does not support brightness control\n",
			output->name);
		client->local_error = true;
		return;
	}
	if (client->requested_brightness < output->min_brightness ||
			client->requested_brightness > output->max_brightness) {
		fprintf(stderr, "brightness %u is outside the range [%u, %u] "
			"for output %s\n", client->requested_brightness,
			output->min_brightness, output->max_brightness, output->name);
		client->local_error = true;
		return;
	}
	wsm_brightness_control_v1_set_brightness(control,
		client->requested_brightness);
	client->set_sent = true;
}

static void brightness_failed(void *data,
		struct wsm_brightness_control_v1 *control) {
	struct output_state *output = data;
	struct client_state *client = output->client;
	if (!output->initialized && !output->failed) {
		client->pending_initializations--;
	}
	output->failed = true;
	client->set_failed = client->set_sent;
	fprintf(stderr, "brightness operation failed for output %s\n",
		output->name ? output->name : "<unknown>");
}

static const struct wsm_brightness_control_v1_listener brightness_listener = {
	.brightness = brightness,
	.min_brightness = min_brightness,
	.max_brightness = max_brightness,
	.method = brightness_method,
	.done = brightness_done,
	.failed = brightness_failed,
};

static void destroy_client(struct client_state *client) {
	struct output_state *output = client->outputs;
	while (output) {
		struct output_state *next = output->next;
		if (output->brightness_control) {
			wsm_brightness_control_v1_destroy(output->brightness_control);
		}
		if (output->output) {
			if (wl_proxy_get_version((struct wl_proxy *)output->output) >= 3) {
				wl_output_release(output->output);
			} else {
				wl_output_destroy(output->output);
			}
		}
		free(output->name);
		free(output);
		output = next;
	}
	if (client->manager) {
		wsm_brightness_control_manager_v1_destroy(client->manager);
	}
	if (client->registry) {
		wl_registry_destroy(client->registry);
	}
	if (client->display) {
		wl_display_disconnect(client->display);
	}
}

static void usage(FILE *stream, const char *program) {
	fprintf(stream,
		"Usage: %s [--output OUTPUT_NAME [--brightness VALUE]]\n"
		"\n"
		"Without arguments, print brightness information for every output.\n"
		"With --output, print only the named output. With --brightness, set\n"
		"that output to VALUE after its initial state has been received.\n",
		program);
}

static bool parse_brightness(const char *text, uint32_t *value) {
	errno = 0;
	char *end = NULL;
	unsigned long parsed = strtoul(text, &end, 10);
	if (errno || end == text || *end != '\0' || parsed > UINT32_MAX) {
		return false;
	}
	*value = parsed;
	return true;
}

int main(int argc, char **argv) {
	struct client_state client = {0};
	static const struct option options[] = {
		{"output", required_argument, NULL, 'o'},
		{"brightness", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{0},
	};
	int option;
	while ((option = getopt_long(argc, argv, "o:b:h", options, NULL)) != -1) {
		switch (option) {
		case 'o':
			client.requested_output = optarg;
			break;
		case 'b':
			if (!parse_brightness(optarg, &client.requested_brightness)) {
				fprintf(stderr, "invalid brightness value: %s\n", optarg);
				return EXIT_FAILURE;
			}
			client.brightness_requested = true;
			break;
		case 'h':
			usage(stdout, argv[0]);
			return EXIT_SUCCESS;
		default:
			usage(stderr, argv[0]);
			return EXIT_FAILURE;
		}
	}
	if (optind != argc || (client.brightness_requested &&
			!client.requested_output)) {
		usage(stderr, argv[0]);
		return EXIT_FAILURE;
	}

	client.display = wl_display_connect(NULL);
	if (!client.display) {
		fprintf(stderr, "failed to connect to the Wayland display\n");
		return EXIT_FAILURE;
	}
	client.registry = wl_display_get_registry(client.display);
	wl_registry_add_listener(client.registry, &registry_listener, &client);
	if (wl_display_roundtrip(client.display) < 0 ||
			wl_display_roundtrip(client.display) < 0 || client.local_error) {
		fprintf(stderr, "failed to enumerate Wayland globals\n");
		destroy_client(&client);
		return EXIT_FAILURE;
	}
	if (!client.manager) {
		fprintf(stderr, "compositor does not advertise "
			"wsm_brightness_control_manager_v1\n");
		destroy_client(&client);
		return EXIT_FAILURE;
	}

	for (struct output_state *output = client.outputs;
			output; output = output->next) {
		if (!output->name) {
			continue;
		}
		if (client.requested_output &&
				strcmp(client.requested_output, output->name) != 0) {
			continue;
		}
		client.target_found = true;
		output->brightness_control =
			wsm_brightness_control_manager_v1_get_output_brightness(
				client.manager, output->output);
		wsm_brightness_control_v1_add_listener(output->brightness_control,
			&brightness_listener, output);
		client.pending_initializations++;
	}
	if (!client.target_found) {
		fprintf(stderr, "%soutput not found%s%s\n",
			client.requested_output ? "" : "no ",
			client.requested_output ? ": " : "",
			client.requested_output ? client.requested_output : "s");
		destroy_client(&client);
		return EXIT_FAILURE;
	}

	while (client.pending_initializations > 0 &&
			wl_display_dispatch(client.display) >= 0) {
	}
	if (client.pending_initializations > 0) {
		fprintf(stderr, "Wayland connection closed during initialization\n");
		client.local_error = true;
	}
	if (client.set_sent) {
		if (wl_display_roundtrip(client.display) < 0) {
			client.local_error = true;
		} else if (!client.set_failed) {
			printf("set output=%s brightness=%u\n", client.requested_output,
				client.requested_brightness);
		}
	}
	bool failed = client.local_error || client.set_failed;
	destroy_client(&client);
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
