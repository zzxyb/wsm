#include "renderer/gles/render_timer.h"
#include "renderer/gles/renderer.h"
#include "renderer/gles/egl.h"
#include "util/wsm_log.h"
#include "util/wsm_time.h"

#include <stdlib.h>
#include <assert.h>

static int gles2_get_render_time(struct wlr_render_timer *wlr_timer) {
	struct wsm_gles2_render_timer *timer = wsm_gles2_render_timer_from_timer(wlr_timer);
	struct wsm_gles2_renderer *renderer = timer->renderer;

	struct wsm_egl_context prev_ctx;
	wsm_egl_make_current(renderer->egl, &prev_ctx);

	GLint64 disjoint;
	renderer->procs.glGetInteger64vEXT(GL_GPU_DISJOINT_EXT, &disjoint);
	if (disjoint) {
		wsm_log(WSM_ERROR, "a disjoint operation occurred and the render timer is invalid");
		wsm_egl_restore_context(&prev_ctx);
		return -1;
	}

	GLint available;
	renderer->procs.glGetQueryObjectivEXT(timer->id,
		GL_QUERY_RESULT_AVAILABLE_EXT, &available);
	if (!available) {
		wsm_log(WSM_ERROR, "timer was read too early, gpu isn't done!");
		wsm_egl_restore_context(&prev_ctx);
		return -1;
	}

	GLuint64 gl_render_end;
	renderer->procs.glGetQueryObjectui64vEXT(timer->id, GL_QUERY_RESULT_EXT,
		&gl_render_end);

	int64_t cpu_nsec_total = timespec_to_nsec(&timer->cpu_end) - timespec_to_nsec(&timer->cpu_start);

	wsm_egl_restore_context(&prev_ctx);
	return gl_render_end - timer->gl_cpu_end + cpu_nsec_total;
}

static void gles2_render_timer_destroy(struct wlr_render_timer *wlr_timer) {
	struct wsm_gles2_render_timer *timer = wl_container_of(wlr_timer, timer, base);
	struct wsm_gles2_renderer *renderer = timer->renderer;

	struct wsm_egl_context prev_ctx;
	wsm_egl_make_current(renderer->egl, &prev_ctx);
	renderer->procs.glDeleteQueriesEXT(1, &timer->id);
	wsm_egl_restore_context(&prev_ctx);
	free(timer);
}

static const struct wlr_render_timer_impl render_timer_impl = {
	.get_duration_ns = gles2_get_render_time,
	.destroy = gles2_render_timer_destroy,
};


struct wsm_gles2_render_timer *wsm_gles2_render_timer_create(struct wlr_renderer *wlr_renderer) {
	struct wsm_gles2_renderer *renderer = wsm_gles2_renderer_from_renderer(wlr_renderer);
	if (!renderer->exts.EXT_disjoint_timer_query) {
		wsm_log(WSM_ERROR, "can't create timer, EXT_disjoint_timer_query not available");
		return NULL;
	}

	struct wsm_gles2_render_timer *timer = calloc(1, sizeof(*timer));
	if (!timer) {
		return NULL;
	}
	timer->base.impl = &render_timer_impl;
	timer->renderer = renderer;

	struct wsm_egl_context prev_ctx;
	wsm_egl_make_current(renderer->egl, &prev_ctx);
	renderer->procs.glGenQueriesEXT(1, &timer->id);
	wsm_egl_restore_context(&prev_ctx);

	return timer;
}

bool wlr_render_timer_is_wsm_gles2(const struct wlr_render_timer *timer) {
	return timer != NULL && timer->impl == &render_timer_impl;
}

struct wsm_gles2_render_timer *wsm_gles2_render_timer_from_timer(struct wlr_render_timer *wlr_timer) {
	assert(wlr_timer->impl == &render_timer_impl);
	struct wsm_gles2_render_timer *timer = wl_container_of(wlr_timer, timer, base);
	return timer;
}
