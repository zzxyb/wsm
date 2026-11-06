#include "pass/blit.h"
#include "pass/gles/blit.h"
#include "pass/pixman/blit.h"
#include "pass/gles/pass.h"
#include "pass/pixman/pass.h"

static bool region_valid(struct wlr_texture *texture, const struct wlr_box *box) {
	int64_t x2 = (int64_t)box->x + box->width;
	int64_t y2 = (int64_t)box->y + box->height;
	return box->width != 0 && box->height != 0 && box->x >= 0 && box->y >= 0 &&
		(uint32_t)box->x <= texture->width && (uint32_t)box->y <= texture->height &&
		x2 >= 0 && y2 >= 0 && x2 <= texture->width && y2 <= texture->height &&
		x2 <= INT32_MAX && y2 <= INT32_MAX;
}

bool wsm_render_pass_blit(struct wlr_render_pass *target,
		const struct wsm_blit_texture_options *options) {
	if (target == NULL || options == NULL || options->texture == NULL ||
			options->dst_texture == NULL || options->texture == options->dst_texture ||
			options->texture->renderer != options->dst_texture->renderer ||
			(options->filter_mode != WLR_SCALE_FILTER_BILINEAR &&
			 options->filter_mode != WLR_SCALE_FILTER_NEAREST) ||
			!region_valid(options->texture, &options->src_box) ||
			!region_valid(options->dst_texture, &options->dst_box)) {
		return false;
	}
	if (wlr_render_pass_is_wsm_gles2(target)) {
		return wsm_gles2_render_pass_blit(target, options);
	}
	if (wlr_render_pass_is_wsm_pixman(target)) {
		return wsm_pixman_render_pass_blit(target, options);
	}
	return false;
}
