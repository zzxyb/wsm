#include "wsm_window_preview.h"

#include <math.h>
#include <stdlib.h>

#include <pixman.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/transform.h>

struct wsm_window_preview;

struct preview_buffer {
	struct wl_list link;
	struct wsm_window_preview *preview;
	struct wlr_scene_buffer *source;
	struct wlr_scene_buffer *mirror;
	struct wlr_surface *surface;
	struct wl_listener source_destroy;
	struct wl_listener surface_commit;
	struct wl_listener new_subsurface;
	struct wl_listener frame_done;
};

struct wsm_window_preview {
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *source_tree;
	struct wl_list buffers;
	int source_width;
	int source_height;
	int source_x;
	int source_y;
	struct wlr_box geometry;
	bool geometry_set;
	bool refreshing;
};

static void preview_refresh_sources(struct wsm_window_preview *preview);

static struct wlr_box rounded_box(double x, double y,
		double width, double height) {
	int left = lround(x);
	int top = lround(y);
	int right = lround(x + width);
	int bottom = lround(y + height);
	return (struct wlr_box) {
		.x = left,
		.y = top,
		.width = right > left ? right - left : 1,
		.height = bottom > top ? bottom - top : 1,
	};
}

static void source_size(struct wlr_scene_buffer *source,
		int *width, int *height) {
	if (source->dst_width > 0 && source->dst_height > 0) {
		*width = source->dst_width;
		*height = source->dst_height;
		return;
	}
	*width = source->buffer_width;
	*height = source->buffer_height;
	wlr_output_transform_coords(source->transform, width, height);
}

static void sync_buffer(struct preview_buffer *buffer,
		const pixman_region32_t *damage) {
	struct wlr_scene_buffer *source = buffer->source;
	if (source == NULL) {
		return;
	}
	if (damage != NULL && source->buffer != NULL) {
		wlr_scene_buffer_set_buffer_with_damage(
			buffer->mirror, source->buffer, damage);
	} else {
		wlr_scene_buffer_set_buffer(buffer->mirror, source->buffer);
	}
	wlr_scene_buffer_set_source_box(buffer->mirror, &source->src_box);
	wlr_scene_buffer_set_transform(buffer->mirror, source->transform);
	wlr_scene_buffer_set_filter_mode(buffer->mirror, source->filter_mode);
	wlr_scene_buffer_set_opacity(buffer->mirror, source->opacity);
	pixman_region32_t empty;
	pixman_region32_init(&empty);
	wlr_scene_buffer_set_opaque_region(buffer->mirror, &empty);
	pixman_region32_fini(&empty);
}

static void update_buffer_geometry(struct preview_buffer *buffer) {
	struct wsm_window_preview *preview = buffer->preview;
	if (!preview->geometry_set || buffer->source == NULL ||
			preview->source_width <= 0 || preview->source_height <= 0) {
		return;
	}
	int source_root_x = 0, source_root_y = 0;
	int source_x = 0, source_y = 0;
	wlr_scene_node_coords(
		&preview->source_tree->node, &source_root_x, &source_root_y);
	wlr_scene_node_coords(
		&buffer->source->node, &source_x, &source_y);
	int local_x = preview->source_x + source_x - source_root_x;
	int local_y = preview->source_y + source_y - source_root_y;
	int width = 0, height = 0;
	source_size(buffer->source, &width, &height);
	if (width <= 0 || height <= 0) {
		wlr_scene_node_set_enabled(&buffer->mirror->node, false);
		return;
	}
	double scale_x =
		(double)preview->geometry.width / preview->source_width;
	double scale_y =
		(double)preview->geometry.height / preview->source_height;
	struct wlr_box box = rounded_box(
		preview->geometry.x + local_x * scale_x,
		preview->geometry.y + local_y * scale_y,
		width * scale_x, height * scale_y);
	wlr_scene_node_set_position(&buffer->mirror->node, box.x, box.y);
	wlr_scene_buffer_set_dest_size(
		buffer->mirror, box.width, box.height);
	wlr_scene_node_set_enabled(&buffer->mirror->node,
		buffer->source->node.enabled && buffer->source->buffer != NULL);
}

static void update_geometry(struct wsm_window_preview *preview) {
	struct preview_buffer *buffer;
	wl_list_for_each(buffer, &preview->buffers, link) {
		update_buffer_geometry(buffer);
	}
}

static void preview_buffer_destroy(struct preview_buffer *buffer,
		bool destroy_node) {
	if (!wl_list_empty(&buffer->source_destroy.link)) {
		wl_list_remove(&buffer->source_destroy.link);
	}
	if (!wl_list_empty(&buffer->surface_commit.link)) {
		wl_list_remove(&buffer->surface_commit.link);
	}
	if (!wl_list_empty(&buffer->new_subsurface.link)) {
		wl_list_remove(&buffer->new_subsurface.link);
	}
	if (!wl_list_empty(&buffer->frame_done.link)) {
		wl_list_remove(&buffer->frame_done.link);
	}
	wl_list_remove(&buffer->link);
	if (destroy_node && buffer->mirror != NULL) {
		wlr_scene_node_destroy(&buffer->mirror->node);
	}
	free(buffer);
}

static void handle_source_destroy(struct wl_listener *listener, void *data) {
	struct preview_buffer *buffer = wl_container_of(
		listener, buffer, source_destroy);
	preview_buffer_destroy(buffer, true);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct preview_buffer *buffer = wl_container_of(
		listener, buffer, surface_commit);
	sync_buffer(buffer, &buffer->surface->buffer_damage);
	preview_refresh_sources(buffer->preview);
	/* A parent commit can reposition several synchronized subsurfaces. */
	update_geometry(buffer->preview);
}

static void handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct preview_buffer *buffer = wl_container_of(
		listener, buffer, new_subsurface);
	preview_refresh_sources(buffer->preview);
}

static void handle_frame_done(struct wl_listener *listener, void *data) {
	struct preview_buffer *buffer = wl_container_of(
		listener, buffer, frame_done);
	if (buffer->surface != NULL) {
		wlr_surface_send_frame_done(buffer->surface, data);
	}
}

static struct preview_buffer *find_buffer(
		struct wsm_window_preview *preview,
		struct wlr_scene_buffer *source) {
	struct preview_buffer *buffer;
	wl_list_for_each(buffer, &preview->buffers, link) {
		if (buffer->source == source) {
			return buffer;
		}
	}
	return NULL;
}

static bool add_buffer(struct wsm_window_preview *preview,
		struct wlr_scene_buffer *source) {
	if (find_buffer(preview, source) != NULL) {
		return true;
	}
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(source);
	if (scene_surface == NULL) {
		return true;
	}
	struct preview_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return false;
	}
	wl_list_init(&buffer->link);
	wl_list_init(&buffer->source_destroy.link);
	wl_list_init(&buffer->surface_commit.link);
	wl_list_init(&buffer->new_subsurface.link);
	wl_list_init(&buffer->frame_done.link);
	buffer->preview = preview;
	buffer->source = source;
	buffer->surface = scene_surface->surface;
	buffer->mirror = wlr_scene_buffer_create(preview->tree, source->buffer);
	if (buffer->mirror == NULL) {
		free(buffer);
		return false;
	}

	buffer->source_destroy.notify = handle_source_destroy;
	wl_signal_add(&source->node.events.destroy, &buffer->source_destroy);
	buffer->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&buffer->surface->events.commit, &buffer->surface_commit);
	buffer->new_subsurface.notify = handle_new_subsurface;
	wl_signal_add(
		&buffer->surface->events.new_subsurface, &buffer->new_subsurface);
	buffer->frame_done.notify = handle_frame_done;
	wl_signal_add(&buffer->mirror->events.frame_done, &buffer->frame_done);
	wl_list_insert(preview->buffers.prev, &buffer->link);
	sync_buffer(buffer, NULL);
	update_buffer_geometry(buffer);
	return true;
}

struct refresh_data {
	struct wsm_window_preview *preview;
	bool ok;
};

static void refresh_iterator(struct wlr_scene_buffer *source,
		int sx, int sy, void *data) {
	struct refresh_data *refresh = data;
	if (!add_buffer(refresh->preview, source)) {
		refresh->ok = false;
	}
}

static void preview_refresh_sources(struct wsm_window_preview *preview) {
	if (preview->refreshing || preview->source_tree == NULL) {
		return;
	}
	preview->refreshing = true;
	struct refresh_data data = {
		.preview = preview,
		.ok = true,
	};
	wlr_scene_node_for_each_buffer(
		&preview->source_tree->node, refresh_iterator, &data);
	preview->refreshing = false;
}

struct wsm_window_preview *wsm_window_preview_create(
		struct wlr_scene_tree *parent, struct wlr_scene_tree *source_tree,
		int source_width, int source_height, int source_x, int source_y) {
	if (parent == NULL || source_tree == NULL ||
			source_width <= 0 || source_height <= 0) {
		return NULL;
	}
	struct wsm_window_preview *preview = calloc(1, sizeof(*preview));
	if (preview == NULL) {
		return NULL;
	}
	wl_list_init(&preview->buffers);
	preview->tree = wlr_scene_tree_create(parent);
	if (preview->tree == NULL) {
		free(preview);
		return NULL;
	}
	preview->source_tree = source_tree;
	preview->source_width = source_width;
	preview->source_height = source_height;
	preview->source_x = source_x;
	preview->source_y = source_y;
	preview_refresh_sources(preview);
	if (wl_list_empty(&preview->buffers)) {
		wsm_window_preview_destroy(preview);
		return NULL;
	}
	return preview;
}

void wsm_window_preview_set_geometry(struct wsm_window_preview *preview,
		const struct wlr_box *box) {
	if (preview == NULL || box == NULL) {
		return;
	}
	preview->geometry = *box;
	preview->geometry_set = true;
	update_geometry(preview);
}

void wsm_window_preview_destroy(struct wsm_window_preview *preview) {
	if (preview == NULL) {
		return;
	}
	struct preview_buffer *buffer, *tmp;
	wl_list_for_each_safe(buffer, tmp, &preview->buffers, link) {
		preview_buffer_destroy(buffer, false);
	}
	if (preview->tree != NULL) {
		wlr_scene_node_destroy(&preview->tree->node);
	}
	free(preview);
}
