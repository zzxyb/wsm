/**
 * @file        wsm_renderer.h
 * @brief       Renderer creation helpers for the wsm compositor.
 * @details     This file exposes the wsm renderer entry points for automatic
 *              backend selection and DRM device based creation.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_WSM_RENDERER_H
#define RENDERER_WSM_RENDERER_H

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>

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
