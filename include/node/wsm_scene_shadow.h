/**
 * @file        wsm_scene_shadow.h
 * @brief       Shadow scene node and shared texture cache.
 * @details     Defines nine-slice shadow geometry, cache ownership, and
 *              helpers for updating the shadow's size, shape, and opacity.
 * @author      YaoBing Xiao
 * @date        2026-10-22
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-10-22, initial version
 */

#ifndef NODE_WSM_SCENE_SHADOW_H
#define NODE_WSM_SCENE_SHADOW_H

#include "node/wsm_scene_node.h"

#include <stddef.h>
#include <stdint.h>

struct wlr_renderer;
struct wlr_texture;

/**
 * @brief Texture slice sizes used to stretch a cached shadow.
 */
struct wsm_scene_shadow_slices {
	int left, right, top, bottom; /**< Edge slice sizes in texture pixels. */
};

/**
 * @brief Space occupied by a shadow around its inner rectangle.
 */
struct wsm_scene_shadow_margins {
	int left, right, top, bottom; /**< Outer margins in logical pixels. */
};

/**
 * @brief Shared nine-slice shadow texture and its renderer lifetime state.
 */
struct wsm_scene_shadow_cache {
	struct wlr_texture *texture; /**< Texture owned by the cache. */
	int width, height; /**< Texture dimensions in pixels. */
	struct wsm_scene_shadow_slices slices; /**< Nine-slice boundaries. */
	struct wl_listener renderer_destroy; /**< Renderer destruction listener. */
	struct wl_list nodes; /**< List of wsm_scene_shadow.cache_link entries. */
	size_t ref_count; /**< Number of owners holding this cache. */
	void *owned_pixels; /**< Pixels retained for buffer-backed textures. */
};

/**
 * @brief Shadow node positioned at the top-left of its outer margins.
 */
struct wsm_scene_shadow {
	struct wsm_scene_node node; /**< Base scene node. */
	struct wsm_scene_shadow_cache *cache; /**< Referenced shadow texture cache. */
	struct wl_list cache_link; /**< Link in the cache's nodes list. */
	int width, height; /**< Inner rectangle dimensions in logical pixels. */
	struct wsm_scene_shadow_margins margins; /**< Space outside the inner rectangle. */
	float top_left_radius, top_right_radius; /**< Top inner corner radii. */
	float bottom_left_radius, bottom_right_radius; /**< Bottom inner corner radii. */
	float inner_offset; /**< Distance the shadow extends inside its outline. */
	float opacity; /**< Node opacity in the range [0, 1]. */
	pixman_region32_t painted_region; /**< Last shadow footprint in scene coordinates. */
};

/**
 * @brief Copies pixels into a new shared shadow texture cache.
 * @param renderer Renderer that owns the resulting texture.
 * @param format DRM pixel format of the input data.
 * @param stride Row stride in bytes.
 * @param width Texture width in pixels.
 * @param height Texture height in pixels.
 * @param data Pixel data to copy.
 * @param slices Nine-slice boundaries in texture pixels.
 * @return New cache, or NULL on invalid input or allocation failure.
 */
struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create(
	struct wlr_renderer *renderer, uint32_t format, uint32_t stride,
	uint32_t width, uint32_t height, const void *data,
	const struct wsm_scene_shadow_slices *slices);

/**
 * @brief Wraps a texture in a shared shadow cache.
 * @param texture Texture transferred to the cache on success.
 * @param slices Nine-slice boundaries in texture pixels.
 * @return New cache, or NULL on invalid input or allocation failure.
 */
struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_from_texture(
	struct wlr_texture *texture, const struct wsm_scene_shadow_slices *slices);

/**
 * @brief Generates a shadow cache with one radius for all four corners.
 * @param renderer GLES2 or Pixman renderer that owns the cache.
 * @param margins Shadow extent on each side in texture pixels.
 * @param corner_radius Radius of every corner in texture pixels.
 * @param inner_offset Distance the shadow extends inside its outline.
 * @param color RGBA shadow color; the generated texture is premultiplied.
 * @return New cache, or NULL on invalid input or creation failure.
 */
struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_rounded(
	struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
	float corner_radius, float inner_offset, const float color[static 4]);

/**
 * @brief Generates a shadow cache with four independent corner radii.
 * @param renderer GLES2 or Pixman renderer that owns the cache.
 * @param margins Shadow extent on each side in texture pixels.
 * @param corner_extent Fixed corner area containing every radius.
 * @param radii Top-left, top-right, bottom-left, bottom-right radii.
 * @param inner_offset Distance the shadow extends inside its outline.
 * @param color RGBA shadow color; the generated texture is premultiplied.
 * @return New cache, or NULL on invalid input or creation failure.
 */
struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_create_rounded_corners(
	struct wlr_renderer *renderer, const struct wsm_scene_shadow_slices *margins,
	float corner_extent, const float radii[static 4], float inner_offset,
	const float color[static 4]);

/**
 * @brief Retains a shared shadow cache.
 * @param cache Cache to retain.
 * @return The same cache with its reference count increased.
 */
struct wsm_scene_shadow_cache *wsm_scene_shadow_cache_ref(
	struct wsm_scene_shadow_cache *cache);

/**
 * @brief Releases a shared shadow cache and destroys it at the last reference.
 * @param cache Cache to release, or NULL.
 */
void wsm_scene_shadow_cache_unref(struct wsm_scene_shadow_cache *cache);

/**
 * @brief Adds a shadow node to a scene tree.
 * @param parent Parent scene tree.
 * @param cache Initial cache to retain, or NULL.
 * @param width Inner rectangle width in logical pixels.
 * @param height Inner rectangle height in logical pixels.
 * @param margins Space around the inner rectangle in logical pixels.
 * @param opacity Initial node opacity in the range [0, 1].
 * @return New shadow node, or NULL on allocation failure.
 */
struct wsm_scene_shadow *wsm_scene_shadow_create(struct wsm_scene_tree *parent,
	struct wsm_scene_shadow_cache *cache, int width, int height,
	const struct wsm_scene_shadow_margins *margins, float opacity);

/**
 * @brief Changes the inner rectangle size and clamps its corner radii.
 * @param shadow Shadow node to update.
 * @param width New inner width in logical pixels.
 * @param height New inner height in logical pixels.
 */
void wsm_scene_shadow_set_size(struct wsm_scene_shadow *shadow, int width, int height);

/**
 * @brief Changes the outer margins of a shadow node.
 * @param shadow Shadow node to update.
 * @param margins New margins in logical pixels.
 */
void wsm_scene_shadow_set_margins(struct wsm_scene_shadow *shadow,
	const struct wsm_scene_shadow_margins *margins);

/**
 * @brief Changes the radii of the shadow's inner outline.
 * @param shadow Shadow node to update.
 * @param top_left Top-left radius in logical pixels.
 * @param top_right Top-right radius in logical pixels.
 * @param bottom_left Bottom-left radius in logical pixels.
 * @param bottom_right Bottom-right radius in logical pixels.
 */
void wsm_scene_shadow_set_corner_radius(struct wsm_scene_shadow *shadow,
	float top_left, float top_right, float bottom_left, float bottom_right);

/**
 * @brief Changes how far the shadow extends inside its outline.
 * @param shadow Shadow node to update.
 * @param inner_offset Nonnegative inset in logical pixels.
 */
void wsm_scene_shadow_set_inner_offset(struct wsm_scene_shadow *shadow, float inner_offset);

/**
 * @brief Changes the opacity of a shadow node.
 * @param shadow Shadow node to update.
 * @param opacity New opacity in the range [0, 1].
 */
void wsm_scene_shadow_set_opacity(struct wsm_scene_shadow *shadow, float opacity);

/**
 * @brief Replaces the cache referenced by a shadow node.
 * @param shadow Shadow node to update.
 * @param cache New cache to retain, or NULL to remove the texture.
 */
void wsm_scene_shadow_set_cache(struct wsm_scene_shadow *shadow,
	struct wsm_scene_shadow_cache *cache);

/**
 * @brief Checks whether a scene node is a shadow.
 * @param node Scene node to inspect.
 * @return true when the node is a shadow, false otherwise.
 */
bool wsm_scene_node_is_shadow(const struct wsm_scene_node *node);

/**
 * @brief Casts a shadow scene node to its containing object.
 * @param node Node known to represent a shadow.
 * @return Enclosing shadow node.
 */
struct wsm_scene_shadow *wsm_scene_shadow_from_node(struct wsm_scene_node *node);

#endif // NODE_WSM_SCENE_SHADOW_H
