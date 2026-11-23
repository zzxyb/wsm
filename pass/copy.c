#include "pass/copy.h"
#include "pass/pass.h"

static bool region_valid(struct wlr_texture *texture, int x, int y, int width, int height) {
	return x >= 0 && y >= 0 && width > 0 && height > 0 &&
		(uint64_t)x + width <= texture->width &&
		(uint64_t)y + height <= texture->height;
}

bool wsm_render_pass_copy(struct wlr_render_pass *target,
		const struct wsm_copy_texture_options *options) {
	if (target == NULL || options == NULL || options->texture == NULL ||
			options->dst_texture == NULL || options->texture == options->dst_texture ||
			options->texture->renderer != options->dst_texture->renderer ||
			options->src_level != 0 || options->dst_level != 0 ||
			!region_valid(options->texture, options->src_x, options->src_y,
				options->width, options->height) ||
			!region_valid(options->dst_texture, options->dst_x, options->dst_y,
				options->width, options->height)) {
		return false;
	}
	const struct wsm_render_pass_impl *impl = wsm_render_pass_get_impl(target);
	return impl && impl->copy && impl->copy(target, options);
}
