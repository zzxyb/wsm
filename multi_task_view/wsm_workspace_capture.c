#include "wsm_workspace_capture.h"

#include "wsm_container.h"
#include "wsm_list.h"
#include "wsm_log.h"
#include "wsm_output.h"
#include "wsm_server.h"
#include "wsm_workspace.h"

#include <math.h>
#include <stdlib.h>

#include <drm_fourcc.h>
#include <pixman.h>

#include <wlr/render/allocator.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/transform.h>

struct capture_source {
	struct wl_list link;
	struct wsm_workspace_capture *capture;
	struct wlr_scene_buffer *buffer;
	struct wlr_surface *surface;
	struct wlr_box last_box;
	struct wl_listener node_destroy;
	struct wl_listener surface_commit;
	struct wl_listener new_subsurface;
};

struct wsm_workspace_capture {
	struct wsm_output *output;
	struct wsm_workspace *workspace;
	struct wlr_renderer *renderer;
	struct wlr_swapchain *swapchain;
	struct wlr_scene_buffer *scene_buffer;
	struct wlr_damage_ring damage_ring;
	struct wlr_box source_box;
	struct wlr_box destination;
	int buffer_width;
	int buffer_height;
	struct wl_list sources;
	struct wl_listener frame_done;
	bool include_shell_layers;
	bool include_shell_top;
	bool ignore_shell_lower_root_enabled;
	bool refreshing_sources;
	bool refresh_sources;
	bool dirty;
	bool first_render_logged;
};

struct container_array {
	struct wsm_container **items;
	size_t length;
	size_t capacity;
};

struct render_data {
	struct wsm_workspace_capture *capture;
	struct wlr_render_pass *pass;
	const pixman_region32_t *damage;
	struct wlr_texture **textures;
	size_t textures_len;
	size_t buffer_nodes;
	size_t rect_nodes;
	size_t empty_buffers;
	size_t import_failures;
};

static void refresh_sources(struct wsm_workspace_capture *capture);

static void collect_container(struct wsm_container *container, void *data) {
	struct container_array *array = data;
	if (container->view == NULL || container->node.destroying) {
		return;
	}
	if (array->length == array->capacity) {
		size_t capacity = array->capacity == 0 ? 8 : array->capacity * 2;
		void *items = realloc(array->items, capacity * sizeof(*array->items));
		if (items == NULL) {
			return;
		}
		array->items = items;
		array->capacity = capacity;
	}
	array->items[array->length++] = container;
}

static void collect_workspace_floating_containers(struct wsm_workspace *workspace,
		struct container_array *array) {
	for (int i = 0; i < workspace->floating->length; ++i) {
		struct wsm_container *container = workspace->floating->items[i];
		collect_container(container, array);
	}
}

static struct wlr_box scale_box(struct wsm_workspace_capture *capture,
		int x, int y, int width, int height) {
	double scale_x = (double)capture->buffer_width /
		capture->source_box.width;
	double scale_y = (double)capture->buffer_height /
		capture->source_box.height;
	int left = lround((x - capture->source_box.x) * scale_x);
	int top = lround((y - capture->source_box.y) * scale_y);
	int right = lround((x + width - capture->source_box.x) * scale_x);
	int bottom = lround((y + height - capture->source_box.y) * scale_y);
	return (struct wlr_box) {
		.x = left,
		.y = top,
		.width = right - left,
		.height = bottom - top,
	};
}

static void scene_buffer_size(struct wlr_scene_buffer *buffer,
		int *width, int *height) {
	if (buffer->dst_width > 0 && buffer->dst_height > 0) {
		*width = buffer->dst_width;
		*height = buffer->dst_height;
		return;
	}
	*width = buffer->WLR_PRIVATE.buffer_width;
	*height = buffer->WLR_PRIVATE.buffer_height;
	wlr_output_transform_coords(buffer->transform, width, height);
}

static struct wlr_box source_target_box(
		struct wsm_workspace_capture *capture,
		struct wlr_scene_buffer *buffer) {
	int x = 0, y = 0, width = 0, height = 0;
	wlr_scene_node_coords(&buffer->node, &x, &y);
	scene_buffer_size(buffer, &width, &height);
	return scale_box(capture, x, y, width, height);
}

static void damage_box(struct wsm_workspace_capture *capture,
		const struct wlr_box *box) {
	if (box->width <= 0 || box->height <= 0) {
		return;
	}
	wlr_damage_ring_add_box(&capture->damage_ring, box);
	capture->dirty = true;
	wlr_output_schedule_frame(capture->output->wlr_output);
}

static void destroy_source(struct capture_source *source) {
	if (!wl_list_empty(&source->node_destroy.link)) {
		wl_list_remove(&source->node_destroy.link);
	}
	if (!wl_list_empty(&source->surface_commit.link)) {
		wl_list_remove(&source->surface_commit.link);
	}
	if (!wl_list_empty(&source->new_subsurface.link)) {
		wl_list_remove(&source->new_subsurface.link);
	}
	wl_list_remove(&source->link);
	free(source);
}

static void handle_source_destroy(struct wl_listener *listener, void *data) {
	struct capture_source *source = wl_container_of(
		listener, source, node_destroy);
	damage_box(source->capture, &source->last_box);
	destroy_source(source);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct capture_source *source = wl_container_of(
		listener, source, surface_commit);
	damage_box(source->capture, &source->last_box);
	struct wlr_box box = source_target_box(source->capture, source->buffer);
	damage_box(source->capture, &box);
	source->last_box = box;
}

static void handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct capture_source *source = wl_container_of(
		listener, source, new_subsurface);
	source->capture->refresh_sources = true;
	wlr_damage_ring_add_whole(&source->capture->damage_ring);
	source->capture->dirty = true;
	wlr_output_schedule_frame(source->capture->output->wlr_output);
}

static struct capture_source *find_source(
		struct wsm_workspace_capture *capture,
		struct wlr_scene_buffer *buffer) {
	struct capture_source *source;
	wl_list_for_each(source, &capture->sources, link) {
		if (source->buffer == buffer) {
			return source;
		}
	}
	return NULL;
}

static void observe_tree(struct wsm_workspace_capture *capture,
		struct wlr_scene_tree *tree, bool ignore_root_enabled) {
	if (tree == NULL || (!ignore_root_enabled && !tree->node.enabled)) {
		return;
	}
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled) {
			continue;
		}
		if (node->type == WLR_SCENE_NODE_TREE) {
			observe_tree(capture, wlr_scene_tree_from_node(node),
				false);
			continue;
		}
		if (node->type != WLR_SCENE_NODE_BUFFER) {
			continue;
		}
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		if (find_source(capture, buffer) != NULL) {
			continue;
		}
		struct capture_source *source = calloc(1, sizeof(*source));
		if (source == NULL) {
			continue;
		}
		source->capture = capture;
		source->buffer = buffer;
		source->last_box = source_target_box(capture, buffer);
		wl_list_init(&source->node_destroy.link);
		wl_list_init(&source->surface_commit.link);
		wl_list_init(&source->new_subsurface.link);
		source->node_destroy.notify = handle_source_destroy;
		wl_signal_add(&node->events.destroy, &source->node_destroy);
		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buffer);
		if (scene_surface != NULL) {
			source->surface = scene_surface->surface;
			source->surface_commit.notify = handle_surface_commit;
			wl_signal_add(&source->surface->events.commit,
				&source->surface_commit);
			source->new_subsurface.notify = handle_new_subsurface;
			wl_signal_add(&source->surface->events.new_subsurface,
				&source->new_subsurface);
		}
		wl_list_insert(&capture->sources, &source->link);
	}
}

static void for_each_source_tree(struct wsm_workspace_capture *capture,
		void (*iterator)(struct wsm_workspace_capture *,
			struct wlr_scene_tree *, bool, void *), void *data) {
	if (capture->include_shell_layers) {
		iterator(capture, capture->output->layers.shell_background,
			capture->ignore_shell_lower_root_enabled, data);
		iterator(capture, capture->output->layers.shell_bottom,
			capture->ignore_shell_lower_root_enabled, data);
	}
	if (capture->workspace->current.fullscreen != NULL) {
		iterator(capture, capture->workspace->layers.fullscreen, true, data);
	} else {
		iterator(capture, capture->workspace->layers.non_fullscreen, true, data);
	}
	struct container_array containers = {0};
	collect_workspace_floating_containers(capture->workspace, &containers);
	for (size_t i = 0; i < containers.length; ++i) {
		iterator(capture, containers.items[i]->scene_tree, true, data);
	}
	free(containers.items);
	if (capture->include_shell_layers && capture->include_shell_top) {
		iterator(capture, capture->output->layers.shell_top, false, data);
	}
}

static void observe_tree_iterator(struct wsm_workspace_capture *capture,
		struct wlr_scene_tree *tree, bool ignore_root_enabled, void *data) {
	observe_tree(capture, tree, ignore_root_enabled);
}

static void refresh_sources(struct wsm_workspace_capture *capture) {
	if (capture->refreshing_sources) {
		return;
	}
	capture->refreshing_sources = true;
	for_each_source_tree(capture, observe_tree_iterator, NULL);
	capture->refresh_sources = false;
	capture->refreshing_sources = false;
}

static bool remember_texture(struct render_data *data,
		struct wlr_texture *texture) {
	void *textures = realloc(data->textures,
		(data->textures_len + 1) * sizeof(*data->textures));
	if (textures == NULL) {
		return false;
	}
	data->textures = textures;
	data->textures[data->textures_len++] = texture;
	return true;
}

static void render_tree(struct wsm_workspace_capture *capture,
		struct wlr_scene_tree *tree, bool ignore_root_enabled,
		struct render_data *data) {
	if (tree == NULL ||
			(!ignore_root_enabled && !tree->node.enabled)) {
		return;
	}
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled) {
			continue;
		}
		if (node->type == WLR_SCENE_NODE_TREE) {
			render_tree(capture, wlr_scene_tree_from_node(node),
				false, data);
			continue;
		}
		int x = 0, y = 0;
		wlr_scene_node_coords(node, &x, &y);
		if (node->type == WLR_SCENE_NODE_RECT) {
			struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
			struct wlr_box box = scale_box(
				capture, x, y, rect->width, rect->height);
			if (box.width <= 0 || box.height <= 0) {
				continue;
			}
			wlr_render_pass_add_rect(data->pass,
				&(struct wlr_render_rect_options) {
					.box = box,
					.color = {
						.r = rect->color[0],
						.g = rect->color[1],
						.b = rect->color[2],
						.a = rect->color[3],
					},
					.clip = data->damage,
				});
			data->rect_nodes++;
			continue;
		}
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		if (buffer->buffer == NULL && buffer->WLR_PRIVATE.texture == NULL) {
			data->empty_buffers++;
			continue;
		}
		int width = 0, height = 0;
		scene_buffer_size(buffer, &width, &height);
		struct wlr_box box = scale_box(capture, x, y, width, height);
		if (box.width <= 0 || box.height <= 0) {
			continue;
		}
		struct wlr_texture *texture = buffer->WLR_PRIVATE.texture;
		bool owned = false;
		struct wlr_client_buffer *client_buffer = NULL;
		if (texture == NULL && buffer->buffer != NULL) {
			client_buffer = wlr_client_buffer_get(buffer->buffer);
			if (client_buffer != NULL) {
				texture = client_buffer->texture;
			}
		}
		if (texture == NULL && buffer->buffer != NULL &&
				client_buffer == NULL) {
			texture = wlr_texture_from_buffer(capture->renderer, buffer->buffer);
			owned = texture != NULL;
		}
		if (texture == NULL) {
			data->import_failures++;
			continue;
		}
		if (owned && !remember_texture(data, texture)) {
			wlr_texture_destroy(texture);
			data->import_failures++;
			continue;
		}
		wlr_render_pass_add_texture(data->pass,
			&(struct wlr_render_texture_options) {
				.texture = texture,
				.src_box = buffer->src_box,
				.dst_box = box,
				.transform =
					wlr_output_transform_invert(buffer->transform),
				.alpha = &buffer->opacity,
				.clip = data->damage,
				.filter_mode = buffer->filter_mode,
			});
		data->buffer_nodes++;
	}
}

static void render_tree_iterator(struct wsm_workspace_capture *capture,
		struct wlr_scene_tree *tree, bool ignore_root_enabled, void *user_data) {
	render_tree(capture, tree, ignore_root_enabled, user_data);
}

static void handle_frame_done(struct wl_listener *listener, void *data) {
	struct wsm_workspace_capture *capture = wl_container_of(
		listener, capture, frame_done);
	struct wlr_scene_frame_done_event *event = data;
	struct capture_source *source;
	wl_list_for_each(source, &capture->sources, link) {
		if (source->surface != NULL) {
			wlr_surface_send_frame_done(source->surface, &event->when);
		}
	}
}

struct wsm_workspace_capture *wsm_workspace_capture_create(
		struct wlr_scene_tree *parent, struct wsm_output *output,
		struct wsm_workspace *workspace, const struct wlr_box *source_box,
		const struct wlr_box *destination, struct wlr_renderer *renderer,
		struct wlr_allocator *allocator) {
	return wsm_workspace_capture_create_options(parent, output, workspace,
		source_box, destination, renderer, allocator, true, 1.0f);
}

struct wsm_workspace_capture *wsm_workspace_capture_create_options(
		struct wlr_scene_tree *parent, struct wsm_output *output,
		struct wsm_workspace *workspace, const struct wlr_box *source_box,
		const struct wlr_box *destination, struct wlr_renderer *renderer,
		struct wlr_allocator *allocator, bool include_shell_layers,
		float buffer_scale) {
	return wsm_workspace_capture_create_layer_options(parent, output,
		workspace, source_box, destination, renderer, allocator,
		include_shell_layers, include_shell_layers, false, buffer_scale);
}

struct wsm_workspace_capture *wsm_workspace_capture_create_layer_options(
		struct wlr_scene_tree *parent, struct wsm_output *output,
		struct wsm_workspace *workspace, const struct wlr_box *source_box,
		const struct wlr_box *destination, struct wlr_renderer *renderer,
		struct wlr_allocator *allocator, bool include_shell_layers,
		bool include_shell_top, bool ignore_shell_lower_root_enabled,
		float buffer_scale) {
	if (parent == NULL || output == NULL || workspace == NULL ||
			source_box == NULL || source_box->width <= 0 ||
			source_box->height <= 0 || destination == NULL ||
			destination->width <= 0 || destination->height <= 0 ||
			renderer == NULL || allocator == NULL) {
		return NULL;
	}
	if (buffer_scale <= 0) {
		buffer_scale = 1.0f;
	}
	struct wsm_workspace_capture *capture = calloc(1, sizeof(*capture));
	if (capture == NULL) {
		return NULL;
	}
	capture->output = output;
	capture->workspace = workspace;
	capture->renderer = renderer;
	capture->source_box = *source_box;
	capture->destination = *destination;
	capture->buffer_width = lround(destination->width * buffer_scale);
	capture->buffer_height = lround(destination->height * buffer_scale);
	if (capture->buffer_width <= 0 || capture->buffer_height <= 0) {
		free(capture);
		return NULL;
	}
	capture->include_shell_layers = include_shell_layers;
	capture->include_shell_top = include_shell_top;
	capture->ignore_shell_lower_root_enabled =
		ignore_shell_lower_root_enabled;
	wl_list_init(&capture->sources);
	wl_list_init(&capture->frame_done.link);
	wlr_damage_ring_init(&capture->damage_ring);

	uint64_t modifier = DRM_FORMAT_MOD_INVALID;
	struct wlr_drm_format format = {
		.format = DRM_FORMAT_ARGB8888,
		.len = 1,
		.capacity = 1,
		.modifiers = &modifier,
	};
	capture->swapchain = wlr_swapchain_create(allocator,
		capture->buffer_width, capture->buffer_height, &format);
	capture->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (capture->swapchain == NULL || capture->scene_buffer == NULL) {
		wsm_log(WSM_ERROR,
			"Workspace capture '%s': could not create %s%s",
			workspace->name,
			capture->swapchain == NULL ? "swapchain" : "",
			capture->scene_buffer == NULL ? " scene buffer" : "");
		wsm_workspace_capture_destroy(capture);
		return NULL;
	}
	wlr_scene_node_set_position(&capture->scene_buffer->node,
		destination->x, destination->y);
	wlr_scene_buffer_set_dest_size(capture->scene_buffer,
		destination->width, destination->height);
	pixman_region32_t empty;
	pixman_region32_init(&empty);
	wlr_scene_buffer_set_opaque_region(capture->scene_buffer, &empty);
	pixman_region32_fini(&empty);
	wlr_scene_node_set_enabled(&capture->scene_buffer->node, false);
	capture->frame_done.notify = handle_frame_done;
	wl_signal_add(&capture->scene_buffer->events.frame_done,
		&capture->frame_done);
	refresh_sources(capture);
	capture->dirty = true;
	wlr_output_schedule_frame(output->wlr_output);
	return capture;
}

bool wsm_workspace_capture_render(struct wsm_workspace_capture *capture) {
	if (capture == NULL || !capture->dirty) {
		return true;
	}
	if (capture->refresh_sources) {
		refresh_sources(capture);
	}
	struct wlr_buffer *buffer =
		wlr_swapchain_acquire(capture->swapchain);
	if (buffer == NULL) {
		wsm_log(WSM_ERROR,
			"Workspace capture '%s': could not acquire swapchain buffer",
			capture->workspace->name);
		return false;
	}
	pixman_region32_t damage;
	pixman_region32_init(&damage);
	wlr_damage_ring_rotate_buffer(&capture->damage_ring, buffer, &damage);
	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(capture->renderer, buffer, NULL);
	if (pass == NULL) {
		wsm_log(WSM_ERROR,
			"Workspace capture '%s': could not begin render pass",
			capture->workspace->name);
		pixman_region32_fini(&damage);
		wlr_buffer_unlock(buffer);
		wlr_damage_ring_add_whole(&capture->damage_ring);
		return false;
	}
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options) {
		.box = {
			.width = capture->buffer_width,
			.height = capture->buffer_height,
		},
		.color = { .r = 0, .g = 0, .b = 0, .a = 0 },
		.clip = &damage,
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});
	struct render_data data = {
		.capture = capture,
		.pass = pass,
		.damage = &damage,
	};
	for_each_source_tree(capture, render_tree_iterator, &data);
	bool submitted = wlr_render_pass_submit(pass);
	for (size_t i = 0; i < data.textures_len; ++i) {
		wlr_texture_destroy(data.textures[i]);
	}
	free(data.textures);
	if (!capture->first_render_logged) {
		size_t sources = 0;
		struct capture_source *source;
		wl_list_for_each(source, &capture->sources, link) {
			sources++;
		}
		wsm_log(WSM_DEBUG, "Workspace capture diagnostic '%s': sources=%zu, "
			"textures=%zu, rects=%zu, empty_buffers=%zu, "
			"import_failures=%zu, pass_submitted=%s",
			capture->workspace->name, sources, data.buffer_nodes,
			data.rect_nodes, data.empty_buffers, data.import_failures,
			submitted ? "yes" : "no");
		capture->first_render_logged = true;
	}
	if (!submitted) {
		wsm_log(WSM_ERROR,
			"Workspace capture '%s': render failed",
			capture->workspace->name);
		pixman_region32_fini(&damage);
		wlr_buffer_unlock(buffer);
		wlr_damage_ring_add_whole(&capture->damage_ring);
		return false;
	}
	wlr_scene_buffer_set_buffer_with_damage(
		capture->scene_buffer, buffer, &damage);
	wlr_scene_node_set_enabled(&capture->scene_buffer->node, true);
	wlr_buffer_unlock(buffer);
	pixman_region32_fini(&damage);
	capture->dirty = false;
	return true;
}

bool wsm_workspace_capture_is_dirty(
		const struct wsm_workspace_capture *capture) {
	return capture != NULL && capture->dirty;
}

void wsm_workspace_capture_set_opacity(
		struct wsm_workspace_capture *capture, float opacity) {
	if (capture != NULL) {
		wlr_scene_buffer_set_opacity(capture->scene_buffer, opacity);
	}
}

void wsm_workspace_capture_destroy(struct wsm_workspace_capture *capture) {
	if (capture == NULL) {
		return;
	}
	struct capture_source *source, *tmp;
	wl_list_for_each_safe(source, tmp, &capture->sources, link) {
		destroy_source(source);
	}
	if (!wl_list_empty(&capture->frame_done.link)) {
		wl_list_remove(&capture->frame_done.link);
	}
	if (capture->scene_buffer != NULL) {
		wlr_scene_node_destroy(&capture->scene_buffer->node);
	}
	wlr_swapchain_destroy(capture->swapchain);
	wlr_damage_ring_finish(&capture->damage_ring);
	free(capture);
}
