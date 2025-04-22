#include "wsm_brightness_control_v1.h"
#include "ext-brightness-control-v1-protocol.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_backlight_device.h"

#include <assert.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output.h>

#define BRIGHTNESS_CONTROL_MANGER_V1_VERSION 1

static const struct ext_brightness_control_v1_interface 
	brightness_control_imp;

static struct wsm_brightness_control_v1 *brightness_control_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_brightness_control_v1_interface,
		&brightness_control_imp));
	return wl_resource_get_user_data(resource);
}

static void brightness_control_handle_set_brightness(struct wl_client *client,
		struct wl_resource *resource, uint32_t value) {
	struct wsm_brightness_control_v1 *brightness_control =
			brightness_control_from_resource(resource);
	wsm_brightness_control_v1_set_brightness(brightness_control, value);
}

static void brightness_control_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void brightness_control_destroy(struct wsm_brightness_control_v1 *brightness_control) {
	assert(brightness_control);

	struct  wsm_brightness_control_manager_v1 *manager = brightness_control->manager;
	struct wsm_output *output = brightness_control->output;
	wl_resource_set_user_data(brightness_control->resource, NULL);
	wl_list_remove(&brightness_control->output_destroy_listener.link);
	wl_list_remove(&brightness_control->link);
	free(brightness_control);

	struct wsm_brightness_control_manager_v1_set_brightness_event event = {
		.output = output,
	};
	wl_signal_emit_mutable(&manager->events.set_brightness, &event);
}

static void brightness_control_handle_resource_destroy(struct wl_resource *resource) {
	struct wsm_brightness_control_v1 *control =
		brightness_control_from_resource(resource);
	brightness_control_destroy(control);
}

static const struct ext_brightness_control_v1_interface 
		brightness_control_imp = {
	.set_brightness = brightness_control_handle_set_brightness,
	.destroy = brightness_control_handle_destroy,
};

static const struct ext_brightness_control_manager_v1_interface
	brightness_control_manager_impl;

static struct wsm_brightness_control_manager_v1 *brightness_control_manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_brightness_control_manager_v1_interface,
		&brightness_control_manager_impl));
	return wl_resource_get_user_data(resource);
}

static void brightness_control_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_brightness_control_v1 *brightness_control =
		wl_container_of(listener, brightness_control, output_destroy_listener);
	brightness_control_destroy(brightness_control);
}

static void brightness_control_manager_get_output_brightness(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id, struct wl_resource *output_resource) {
	struct wsm_brightness_control_manager_v1 *manager =
		brightness_control_manager_from_resource(manager_resource);
	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&ext_brightness_control_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &brightness_control_imp,
		NULL, brightness_control_handle_resource_destroy);
	
	struct wlr_output *wlr_output = wlr_output_from_resource(output_resource);
	if (!wlr_output) {
		ext_brightness_control_v1_send_failed(resource);
		return;
	}

	struct wsm_output *wsm_output = wlr_output->data;
	assert(wsm_output);
	if (!wsm_output->backlight_device) {
		ext_brightness_control_v1_send_failed(resource);
		return;
	}

	if (wsm_brightness_control_manager_v1_get_output_brightness(manager, wsm_output) != NULL) {
		ext_brightness_control_v1_send_failed(resource);
		return;
	}

	struct wsm_brightness_control_v1 *brightness_control =
		calloc(1, sizeof(struct wsm_brightness_control_v1));
	if (!brightness_control) {
		wsm_log(WSM_ERROR, "Could not create wsm_brightness_control_v1: allocation failed!");
		wl_client_post_no_memory(client);
		return;
	}

	brightness_control->output = wsm_output;
	brightness_control->manager = manager;
	brightness_control->resource = resource;
	wl_resource_set_user_data(resource, brightness_control);

	brightness_control->output_destroy_listener.notify = brightness_control_handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy,
		&brightness_control->output_destroy_listener);
	wl_list_insert(&manager->controls, &brightness_control->link);
	ext_brightness_control_v1_send_brightness(resource, wsm_output->backlight_device->brightness);
	ext_brightness_control_v1_send_max_brightness(resource, wsm_output->backlight_device->max_brightness);
}

static void brightness_control_manager_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct ext_brightness_control_manager_v1_interface
		brightness_control_manager_impl = {
	.get_output_brightness = brightness_control_manager_get_output_brightness,
	.destroy = brightness_control_manager_destroy,
};

static void brightness_control_manager_client_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wsm_brightness_control_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&ext_brightness_control_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &brightness_control_manager_impl,
		manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wsm_brightness_control_manager_v1 *manager = 
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, manager);

	assert(wl_list_empty(&manager->events.destroy.listener_list));
	assert(wl_list_empty(&manager->events.set_brightness.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wsm_brightness_control_manager_v1 *wsm_brightness_control_manager_v1_create(
		struct wl_display *display) {
	struct wsm_brightness_control_manager_v1 *manager = calloc(1, sizeof(struct wsm_brightness_control_manager_v1));
	if (!manager) {
		wsm_log(WSM_ERROR, "Could not create wsm_brightness_control_manager_v1: allocation failed!");
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_brightness_control_manager_v1_interface,
		BRIGHTNESS_CONTROL_MANGER_V1_VERSION,
		manager,
		brightness_control_manager_client_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.set_brightness);

	wl_list_init(&manager->controls);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}

struct wsm_brightness_control_v1 *wsm_brightness_control_manager_v1_get_output_brightness(
		struct wsm_brightness_control_manager_v1 *manager, struct wsm_output *output) {
	struct wsm_brightness_control_v1 *brightness_control;
	wl_list_for_each(brightness_control, &manager->controls, link) {
		if (brightness_control->output == output) {
			return brightness_control;
		}
	}

	return NULL;
}

bool wsm_brightness_control_v1_set_brightness(
		struct wsm_brightness_control_v1 *brightness_control, long brightness) {
	assert(brightness_control);

	if (!wsm_backlight_device_set_brightness(
		brightness_control->output->backlight_device, brightness)) {
		return false;
	}

	struct wsm_brightness_control_manager_v1_set_brightness_event event = {
		.output = brightness_control->output,
		.control = brightness_control,
	};
	wl_signal_emit_mutable(&brightness_control->manager->events.set_brightness, &event);

	return true;
}

void wsm_brightness_control_v1_failed_and_destroy(
		struct wsm_brightness_control_v1 *brightness_control) {
	assert(brightness_control);

	ext_brightness_control_v1_send_failed(brightness_control->resource);
	brightness_control_destroy(brightness_control);
}