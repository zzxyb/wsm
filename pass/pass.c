#include "pass/pass.h"

void wsm_render_pass_init(struct wsm_render_pass *pass,
		const struct wsm_render_pass_impl *impl) {
	wlr_render_pass_init(&pass->base, &impl->base);
}

const struct wsm_render_pass_impl *wsm_render_pass_get_impl(const struct wlr_render_pass *base) {
	const struct wsm_render_pass_impl *impl = wl_container_of(base->impl, impl, base);
	return impl;
}

struct wsm_render_pass *wsm_render_pass_from_base(struct wlr_render_pass *base) {
	struct wsm_render_pass *pass = wl_container_of(base, pass, base);
	return pass;
}
