#include "pass/copy.h"
#include "pass/gles/copy.h"
#include "renderer/gles/renderer.h"
#include "pass/pixman/copy.h"
#include "renderer/pixman/renderer.h"
#include <assert.h>

static void handle_renderer_destroy(struct wl_listener *listener, void *data) {
	struct wsm_render_pass_copy *pass = wl_container_of(listener, pass, renderer_destroy);
	wsm_render_pass_copy_destroy(pass);
}

void wsm_render_pass_copy_init(struct wsm_render_pass_copy *pass,
		const struct wsm_copy_texture_impl *impl, struct wlr_renderer *renderer) {
	assert(impl != NULL && impl->copy != NULL && impl->destroy != NULL && renderer != NULL);
	*pass = (struct wsm_render_pass_copy){ .impl = impl, .renderer = renderer };
	pass->renderer_destroy.notify = handle_renderer_destroy;
	wl_signal_add(&renderer->events.destroy, &pass->renderer_destroy);
}

struct wsm_render_pass_copy *wsm_render_pass_copy_autocreate(struct wlr_renderer *renderer) {
	if (renderer == NULL) {
		return NULL;
	}
	if (wlr_renderer_is_wsm_gles2(renderer)) {
		return wsm_render_pass_copy_gles_create(renderer);
	} else if (wlr_renderer_is_wsm_pixman(renderer)) {
		return wsm_render_pass_copy_pixman_create(renderer);
	}
	return NULL;
}

void wsm_render_pass_copy_destroy(struct wsm_render_pass_copy *pass) {
	if (pass == NULL) {
		return;
	}
	wl_list_remove(&pass->renderer_destroy.link);
	pass->impl->destroy(pass);
}

static bool region_valid(struct wlr_texture *texture, int x, int y, int width, int height) {
	return x >= 0 && y >= 0 && width > 0 && height > 0 &&
		(uint64_t)x + width <= texture->width &&
		(uint64_t)y + height <= texture->height;
}

bool wsm_render_pass_copy(struct wlr_render_pass *target, struct wsm_render_pass_copy *pass,
		const struct wsm_copy_texture_options *options) {
	if (target == NULL || pass == NULL || options == NULL || options->texture == NULL ||
			options->dst_texture == NULL || options->texture == options->dst_texture ||
			options->texture->renderer != pass->renderer ||
			options->dst_texture->renderer != pass->renderer ||
			options->src_level != 0 || options->dst_level != 0 ||
			!region_valid(options->texture, options->src_x, options->src_y,
				options->width, options->height) ||
			!region_valid(options->dst_texture, options->dst_x, options->dst_y,
				options->width, options->height)) {
		return false;
	}
	return pass->impl->copy(target, options);
}
