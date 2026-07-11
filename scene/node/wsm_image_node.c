#include "wsm_image_node.h"
#include "wsm_log.h"
#include "wsm_server.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <cairo.h>
#include <librsvg/rsvg.h>
#include <png.h>
#include <jpeglib.h>
#include <X11/xpm.h>

#include <drm_fourcc.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/interfaces/wlr_buffer.h>

struct cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

struct image_buffer {
	struct wl_listener outputs_update;
	struct wl_listener destroy;
	struct wsm_image_node props;
	struct wlr_scene_buffer *buffer_node;
	struct cairo_buffer *buffer;
	char *path;

	enum wl_output_subpixel subpixel;
	float scale;

	bool visible;
	int image_width;
	int image_height;
	int dest_width;
	int dest_height;
};

static bool reload_image_buffer(struct image_buffer *buffer);

static void cairo_buffer_handle_destroy(struct wlr_buffer *wlr_buffer) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	cairo_surface_destroy(buffer->surface);
	free(buffer);
}

static bool cairo_buffer_handle_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*data = cairo_image_surface_get_data(buffer->surface);
	*stride = cairo_image_surface_get_stride(buffer->surface);
	*format = DRM_FORMAT_ARGB8888;
	return true;
}

static void cairo_buffer_handle_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
	.destroy = cairo_buffer_handle_destroy,
	.begin_data_ptr_access = cairo_buffer_handle_begin_data_ptr_access,
	.end_data_ptr_access = cairo_buffer_handle_end_data_ptr_access,
};

static int is_target_image(const char *image_path, const char *lower_suffix, const char *capital_suffix) {
	const char *ext = strrchr(image_path, '.');
	if (ext != NULL) {
		if (strcmp(ext, lower_suffix) == 0 || strcmp(ext, capital_suffix) == 0) {
			return 1;
		}
	}
	return 0;
}

static void update_source_box(struct image_buffer *buffer) {
	if (buffer->image_width <= 0 || buffer->image_height <= 0) {
		return;
	}

	struct wlr_fbox source_box = {
		.x = 0,
		.y = 0,
		.width = buffer->image_width,
		.height = buffer->image_height,
	};

	wlr_scene_buffer_set_source_box(buffer->buffer_node, &source_box);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct image_buffer *buffer = wl_container_of(listener, buffer, destroy);

	wl_list_remove(&buffer->outputs_update.link);
	wl_list_remove(&buffer->destroy.link);

	free(buffer->path);
	free(buffer);
}

static void handle_outputs_update(struct wl_listener *listener, void *data) {
	struct image_buffer *buffer = wl_container_of(listener, buffer, outputs_update);
	struct wlr_scene_outputs_update_event *event = data;

	float scale = 0;
	enum wl_output_subpixel subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;

	for (size_t i = 0; i < event->size; i++) {
		struct wlr_scene_output *output = event->active[i];
		if (subpixel == WL_OUTPUT_SUBPIXEL_UNKNOWN) {
			subpixel = output->output->subpixel;
		} else if (subpixel != output->output->subpixel) {
			subpixel = WL_OUTPUT_SUBPIXEL_NONE;
		}

		if (scale != 0 && scale != output->output->scale) {
			subpixel = WL_OUTPUT_SUBPIXEL_NONE;
		}

		if (scale < output->output->scale) {
			scale = output->output->scale;
		}
	}

	buffer->visible = event->size > 0;

	if (scale != buffer->scale || subpixel != buffer->subpixel) {
		buffer->scale = scale;
		buffer->subpixel = subpixel;
		reload_image_buffer(buffer);
	}
}

struct wsm_image_node *wsm_image_node_create(struct wlr_scene_tree *parent,
		int width, int height, char *path, float alpha) {
	struct image_buffer *buffer = calloc(1, sizeof(struct image_buffer));
	if (!buffer) {
		wsm_log(WSM_ERROR, "Could not create image_buffer: allocation failed!");
		return NULL;
	}

	struct wlr_scene_buffer *node = wlr_scene_buffer_create(parent, NULL);
	if (!node) {
		free(buffer);
		return NULL;
	}

	buffer->buffer_node = node;
	wlr_scene_buffer_set_filter_mode(node, WLR_SCALE_FILTER_BILINEAR);
	buffer->props.node_wlr = &node->node;
	if (!path) {
		free(buffer);
		wlr_scene_node_destroy(&node->node);
		return NULL;
	}

	buffer->props.width = width;
	buffer->props.height = height;
	buffer->dest_width = width;
	buffer->dest_height = height;
	buffer->props.alpha = alpha;

	buffer->destroy.notify = handle_destroy;
	wl_signal_add(&node->node.events.destroy, &buffer->destroy);
	buffer->outputs_update.notify = handle_outputs_update;
	wl_signal_add(&node->events.outputs_update, &buffer->outputs_update);
	wsm_image_node_load(&buffer->props, path);
	wlr_scene_buffer_set_dest_size(buffer->buffer_node,
		width, height);
	return &buffer->props;
}

void wsm_image_node_update_alpha(struct wsm_image_node *node, float alpha) {
	struct image_buffer *image_buffer = wl_container_of(node, image_buffer, props);
	image_buffer->buffer_node->opacity = alpha;
}

void handle_jpeg_error(j_common_ptr cinfo) {
	cinfo->err->output_message(cinfo);
	wsm_log(WSM_ERROR, "read jpg file error");
}

static cairo_surface_t *create_cairo_surface_from_file_at_size(
		const char *file_path, int requested_width, int requested_height) {
	cairo_surface_t *surface = NULL;
	if (is_target_image(file_path, ".png", ".PNG")) {
		// PNG image
		surface = cairo_image_surface_create_from_png(file_path);
	} else if (is_target_image(file_path, ".jpg", ".JPG")) {
		// JPEG image
		FILE *infile = fopen(file_path, "rb");
		if (!infile) {
			wsm_log(WSM_ERROR, "Unable to read XPM file: %s", file_path);
			return NULL;
		}

		struct jpeg_decompress_struct cinfo;
		struct jpeg_error_mgr jerr;
		cinfo.err = jpeg_std_error(&jerr);
		jerr.error_exit = handle_jpeg_error;
		jpeg_create_decompress(&cinfo);
		jpeg_stdio_src(&cinfo, infile);
		jpeg_read_header(&cinfo, TRUE);
		jpeg_start_decompress(&cinfo);

		int width = cinfo.output_width;
		int height = cinfo.output_height;
		int row_stride = width * cinfo.output_components;

		unsigned char *buffer = (unsigned char*) malloc(row_stride);
		surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		unsigned char *surface_data = cairo_image_surface_get_data(surface);

		while (cinfo.output_scanline < cinfo.output_height) {
			jpeg_read_scanlines(&cinfo, &buffer, 1);
			for (int x = 0; x < width; x++) {
				unsigned char *pixel = buffer + x * cinfo.output_components;
				unsigned char *surface_pixel = surface_data + (cinfo.output_scanline - 1) * cairo_image_surface_get_stride(surface) + x * 4;
				surface_pixel[2] = pixel[0]; // R
				surface_pixel[1] = pixel[1]; // G
				surface_pixel[0] = pixel[2]; // B
				surface_pixel[3] = 255;      // A
			}
		}

		free(buffer);
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
	} else if (is_target_image(file_path, ".svg", ".SVG")) {
		// SVG image
		GError *error = NULL;
		RsvgHandle *handle = rsvg_handle_new_from_file(file_path, &error);
		if (error) {
			wsm_log(WSM_ERROR, "Unable to read XPM file: %s, error: %s", file_path, error->message);
			g_error_free(error);
			return NULL;
		}

		gdouble out_width = 0;
		gdouble out_height = 0;
		bool has_size = rsvg_handle_get_intrinsic_size_in_pixels(handle,
			&out_width, &out_height);
		if (!has_size || out_width <= 0 || out_height <= 0) {
			gboolean has_viewbox = false;
			RsvgRectangle viewbox = {0};
			rsvg_handle_get_intrinsic_dimensions(handle, NULL, NULL,
				NULL, NULL, &has_viewbox, &viewbox);
			if (has_viewbox && viewbox.width > 0 && viewbox.height > 0) {
				out_width = viewbox.width;
				out_height = viewbox.height;
			}
		}

		if (out_width <= 0 || out_height <= 0) {
			wsm_log(WSM_ERROR, "Unable to determine SVG size: %s", file_path);
			g_object_unref(handle);
			return NULL;
		}
		if (requested_width > 0 && requested_height > 0) {
			out_width = requested_width;
			out_height = requested_height;
		}

		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, out_width, out_height);
		cairo_t *cr = cairo_create(surface);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
		error = NULL;
		RsvgRectangle vieport = {
			.x = 0,
			.y = 0,
			.width = out_width,
			.height= out_height,
		};
		if (!rsvg_handle_render_document(handle, cr, &vieport, &error)) {
			wsm_log(WSM_ERROR, "Unable to render SVG file: %s, error: %s",
				file_path, error ? error->message : "unknown error");
			g_clear_error(&error);
			cairo_destroy(cr);
			cairo_surface_destroy(surface);
			g_object_unref(handle);
			return NULL;
		}
		g_object_unref(handle);
		cairo_destroy(cr);
		cairo_surface_flush(surface);
	} else if (is_target_image(file_path, ".xpm", ".XPM")) {
		XpmImage image;
		XpmInfo info;
		int status = XpmReadFileToXpmImage(file_path, &image, &info);

		if (status != XpmSuccess) {
			wsm_log(WSM_ERROR, "Unable to read XPM file: %s", XpmGetErrorString(status));
			return NULL;
		}

		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, image.width, image.height);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			wsm_log(WSM_ERROR, "Unable to create Cairo image surface");
			XpmFreeXpmImage(&image);
			return NULL;
		}

		unsigned char *data = cairo_image_surface_get_data(surface);
		int stride = cairo_image_surface_get_stride(surface);

		for (unsigned int y = 0; y < image.height; y++) {
			for (unsigned int x = 0; x < image.width; x++) {
				unsigned int pixel = image.data[y * image.width + x];
				unsigned char *p = data + y * stride + x * 4;
				// XPM to RGBA
				p[0] = (pixel & 0xFF000000) >> 24; // R
				p[1] = (pixel & 0x00FF0000) >> 16; // G
				p[2] = (pixel & 0x0000FF00) >> 8;  // B
				p[3] = (pixel & 0x000000FF);       // A
			}
		}

		cairo_surface_mark_dirty(surface);
		XpmFreeXpmImage(&image);
	} else {
		wsm_log(WSM_ERROR, "unsupported image format, file: %s", file_path);
		return NULL;
	}

	if (cairo_surface_status(surface)) {
		wsm_log(WSM_ERROR, "error reading file '%s'", file_path);
		cairo_surface_destroy(surface);
		return NULL;
	}

	return surface;
}

cairo_surface_t *create_cairo_surface_frome_file(const char *file_path) {
	return create_cairo_surface_from_file_at_size(file_path, 0, 0);
}

struct wlr_texture *create_texture_from_cairo_surface(struct wlr_renderer *renderer, cairo_surface_t *surface) {
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);

	struct wlr_texture *texture = wlr_texture_from_pixels(
		renderer,
		DRM_FORMAT_ARGB8888,
		stride,
		width,
		height,
		data
	);

	return texture;
}

struct wlr_texture *create_texture_from_file_v1(struct wlr_renderer *renderer, const char *file_path) {
	cairo_surface_t *surface = create_cairo_surface_frome_file(file_path);
	if (!surface) {
		return NULL;
	}

	cairo_surface_flush(surface);
	return create_texture_from_cairo_surface(renderer, surface);
}

void wsm_image_node_load(struct wsm_image_node *node, const char *file_path) {
	struct image_buffer *image_buffer = wl_container_of(node, image_buffer, props);
	if (image_buffer->path != NULL &&
		strcmp(file_path, image_buffer->path) == 0) {
		return;
	}

	char *new_path = strdup(file_path);
	if (!new_path) {
		return;
	}

	free(image_buffer->path);
	image_buffer->path = new_path;
	reload_image_buffer(image_buffer);
}

static bool reload_image_buffer(struct image_buffer *image_buffer) {
	if (!image_buffer->path) {
		return false;
	}

	struct cairo_buffer *buffer = calloc(1, sizeof(struct cairo_buffer));
	if (!buffer) {
		wsm_log(WSM_ERROR, "Could not create cairo_buffer: allocation failed!");
		return false;
	}

	float scale = image_buffer->scale > 0 ? image_buffer->scale : 1.0f;
	int requested_width = image_buffer->props.width > 0 ?
		ceil(image_buffer->props.width * scale) : 0;
	int requested_height = image_buffer->props.height > 0 ?
		ceil(image_buffer->props.height * scale) : 0;
	buffer->surface = create_cairo_surface_from_file_at_size(image_buffer->path,
		requested_width, requested_height);
	if (!buffer->surface) {
		free(buffer);
		return false;
	}

	image_buffer->buffer = buffer;
	image_buffer->image_width = cairo_image_surface_get_width(buffer->surface);
	image_buffer->image_height = cairo_image_surface_get_height(buffer->surface);
	if (image_buffer->props.width <= 0) {
		image_buffer->props.width = image_buffer->image_width / scale;
	}
	if (image_buffer->props.height <= 0) {
		image_buffer->props.height = image_buffer->image_height / scale;
	}
	cairo_surface_flush(buffer->surface);

	wlr_buffer_init(&buffer->base, &cairo_buffer_impl,
		cairo_image_surface_get_width(buffer->surface),
		cairo_image_surface_get_height(buffer->surface));
	wlr_scene_buffer_set_buffer(image_buffer->buffer_node, &buffer->base);
	wlr_buffer_drop(&buffer->base);
	update_source_box(image_buffer);
	wlr_scene_buffer_set_dest_size(image_buffer->buffer_node,
		image_buffer->dest_width > 0 ? image_buffer->dest_width :
			image_buffer->props.width,
		image_buffer->dest_height > 0 ? image_buffer->dest_height :
			image_buffer->props.height);
	return true;
}

void wsm_image_node_set_size(struct wsm_image_node *node, int width, int height) {
	struct image_buffer *image_buffer = wl_container_of(node, image_buffer, props);
	assert(image_buffer);
	if (image_buffer->buffer_node->dst_width == width &&
		image_buffer->buffer_node->dst_height == height) {
		return;
	}
	image_buffer->dest_width = width;
	image_buffer->dest_height = height;
	wlr_scene_buffer_set_dest_size(image_buffer->buffer_node,
		width, height);
}
