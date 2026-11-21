/* SPDX-License-Identifier: MIT */

/**
 * @file        rounded.h
 * @brief       Region helpers for rounded render pass draws.
 * @details     Defines corner bounds and partitions a clipped destination into
 *              regions for ordinary drawing and rounded coverage.
 *
 * @author      YaoBing Xiao
 * @date        2026-11-21
 * @version     v1.0
 * @par Copyright(c):
 *              See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-21, initial version\n
 */

#ifndef WSM_PASS_ROUNDED_H
#define WSM_PASS_ROUNDED_H

#include <math.h>
#include <pixman.h>
#include <wlr/util/box.h>

/**
 * @brief Computes the destination bounds of one rounded corner.
 * @details Rounds the radius up to whole pixels and caps the bounds at the
 *          corresponding halves of the destination. For odd dimensions,
 *          the center column belongs to the left half and the center row
 *          belongs to the top half.
 * @param box Destination rectangle in buffer coordinates.
 * @param corner Corner index from 0 to 3 in top-left, top-right, bottom-left,
 *               and bottom-right order.
 * @param radius Normalized nonnegative radius in buffer pixels.
 * @return Corner bounds in buffer coordinates, empty when the radius is zero.
 */
static inline struct wlr_box rounded_corner_box(const struct wlr_box *box,
		int corner, float radius) {
	bool right = corner & 1, bottom = corner & 2;
	int size = ceilf(radius);
	int max_width = box->width / 2 + (!right && box->width % 2);
	int max_height = box->height / 2 + (!bottom && box->height % 2);
	int width = size < max_width ? size : max_width;
	int height = size < max_height ? size : max_height;
	return (struct wlr_box){
		.x = right ? box->x + (box->width - width) : box->x,
		.y = bottom ? box->y + (box->height - height) : box->y,
		.width = width, .height = height,
	};
}

/**
 * @brief Splits a clipped destination into ordinary and rounded draw regions.
 * @details The output regions are disjoint and together cover the clipped
 *          destination. Finalize both regions with pixman_region32_fini().
 * @param box Destination rectangle in buffer coordinates.
 * @param radii Normalized radii in buffer pixels, in top-left, top-right,
 *              bottom-left, and bottom-right order.
 * @param clip Optional clip region in buffer coordinates, or NULL.
 * @param[out] solid Uninitialized region receiving the area for ordinary draws.
 * @param[out] corners Uninitialized region receiving the area for rounded draws.
 */
static inline void split_rounded_region(const struct wlr_box *box,
		const float radii[static 4], const pixman_region32_t *clip,
		pixman_region32_t *solid, pixman_region32_t *corners) {
	pixman_region32_init_rect(solid, box->x, box->y, box->width, box->height);
	if (clip) {
		pixman_region32_intersect(solid, solid, clip);
	}
	if (!pixman_region32_not_empty(solid)) {
		pixman_region32_init(corners);
		return;
	}
	const pixman_box32_t *extents = pixman_region32_extents(solid);
	pixman_box32_t boxes[4];
	int count = 0;
	for (int i = 0; i < 4; i++) {
		struct wlr_box corner = rounded_corner_box(box, i, radii[i]);
		pixman_box32_t bounds = {
			.x1 = corner.x > extents->x1 ? corner.x : extents->x1,
			.y1 = corner.y > extents->y1 ? corner.y : extents->y1,
			.x2 = corner.x + corner.width,
			.y2 = corner.y + corner.height,
		};
		bounds.x2 = bounds.x2 < extents->x2 ? bounds.x2 : extents->x2;
		bounds.y2 = bounds.y2 < extents->y2 ? bounds.y2 : extents->y2;
		if (bounds.x1 < bounds.x2 && bounds.y1 < bounds.y2) {
			boxes[count++] = bounds;
		}
	}
	pixman_region32_init_rects(corners, boxes, count);
	if (count > 0) {
		pixman_region32_intersect(corners, corners, solid);
		if (pixman_region32_not_empty(corners)) {
			pixman_region32_subtract(solid, solid, corners);
		}
	}
}

#endif
