#include "pass/blit.h"
#include "pass/gles/blit.h"
#include "renderer/gles/renderer.h"
#include "pass/pixman/blit.h"
#include "renderer/pixman/renderer.h"
#include <assert.h>

static void handle_renderer_destroy(struct wl_listener *listener, void *data) {
	struct wsm_render_pass_blit *pass = wl_container_of(listener, pass, renderer_destroy);
	wsm_render_pass_blit_destroy(pass);
}

void wsm_render_pass_blit_init(struct wsm_render_pass_blit *pass,
		const struct wsm_blit_texture_impl *impl, struct wlr_renderer *renderer) {
	assert(impl != NULL && impl->blit != NULL && impl->destroy != NULL && renderer != NULL);
	*pass = (struct wsm_render_pass_blit){ .impl = impl, .renderer = renderer };
	pass->renderer_destroy.notify = handle_renderer_destroy;
	wl_signal_add(&renderer->events.destroy, &pass->renderer_destroy);
}

struct wsm_render_pass_blit *wsm_render_pass_blit_autocreate(struct wlr_renderer *renderer) {
	if (renderer == NULL) {
		return NULL;
	}
	if (wlr_renderer_is_wsm_gles2(renderer)) {
		return wsm_render_pass_blit_gles_create(renderer);
	} else if (wlr_renderer_is_wsm_pixman(renderer)) {
		return wsm_render_pass_blit_pixman_create(renderer);
	}
	return NULL;
}

void wsm_render_pass_blit_destroy(struct wsm_render_pass_blit *pass) {
	if (pass == NULL) {
		return;
	}
	wl_list_remove(&pass->renderer_destroy.link);
	pass->impl->destroy(pass);
}

static bool region_valid(struct wlr_texture *texture, const struct wlr_box *box) {
	int64_t x2 = (int64_t)box->x + box->width;
	int64_t y2 = (int64_t)box->y + box->height;
	return box->width != 0 && box->height != 0 && box->x >= 0 && box->y >= 0 &&
		(uint32_t)box->x <= texture->width && (uint32_t)box->y <= texture->height &&
		x2 >= 0 && y2 >= 0 && x2 <= texture->width && y2 <= texture->height &&
		x2 <= INT32_MAX && y2 <= INT32_MAX;
}

bool wsm_render_pass_blit(struct wlr_render_pass *target, struct wsm_render_pass_blit *pass,
		const struct wsm_blit_texture_options *options) {
	if (target == NULL || pass == NULL || options == NULL || options->texture == NULL ||
			options->dst_texture == NULL || options->texture == options->dst_texture ||
			options->texture->renderer != pass->renderer ||
			options->dst_texture->renderer != pass->renderer ||
			(options->filter_mode != WLR_SCALE_FILTER_BILINEAR &&
			 options->filter_mode != WLR_SCALE_FILTER_NEAREST) ||
			!region_valid(options->texture, &options->src_box) ||
			!region_valid(options->dst_texture, &options->dst_box)) {
		return false;
	}
	return pass->impl->blit(target, options);
}
