/**
 * @file        wsm_renderer.h
 * @brief       Renderer creation helpers and extended implementation callbacks.
 * @details     This file exposes the wsm renderer entry points for automatic
 *              backend selection, DRM device based creation, and shadow cache
 *              creation through backend implementation callbacks.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_WSM_RENDERER_H
#define RENDERER_WSM_RENDERER_H

#include <wlr/render/interface.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>

struct wsm_scene_shadow_cache;
struct wsm_scene_shadow_slices;

/**
 * @brief Virtual operations implemented by a wsm renderer backend.
 * @details Extends the wlroots implementation with shadow cache creation.
 */
struct wsm_renderer_impl {
	struct wlr_renderer_impl base; /**< Base wlroots renderer callbacks. */
	struct wsm_scene_shadow_cache *(*shadow_cache_create)(struct wlr_renderer *renderer,
		const struct wsm_scene_shadow_slices *margins, float corner_extent,
		const float radii[static 4], float inner_offset, const float color[static 4]); /**< Creates a rounded shadow cache, or returns NULL on failure. */
};

/**
 * @brief Gets the extended implementation of a wlroots renderer.
 * @param renderer Renderer using a wsm backend implementation.
 * @return Borrowed pointer to the backend implementation callbacks.
 * @pre The renderer must be non-NULL and use a wsm_renderer_impl.
 */
const struct wsm_renderer_impl *wsm_renderer_get_impl(struct wlr_renderer *renderer);

/**
 * @brief Automatically creates a wsm renderer for a backend.
 * @param backend Backend used to select and initialize the renderer.
 * @return Newly created renderer, or NULL on failure.
 */
struct wlr_renderer *wlr_renderer_autocreate_wsm(struct wlr_backend *backend);

/**
 * @brief Creates a wsm renderer for a DRM file descriptor.
 * @param drm_fd DRM device file descriptor.
 * @return Newly created renderer, or NULL on failure.
 */
struct wlr_renderer *wlr_renderer_autocreate_wsm_with_drm_fd(int drm_fd);

#endif // RENDERER_WSM_RENDERER_H
