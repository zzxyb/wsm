/**
 * @file        rect_union.h
 * @brief       Rectangle accumulation for Vulkan damage tracking.
 * @details     Defines a deferred rectangle union with a bounding box fallback for allocation or
 *              complexity limits.
 * @author      YaoBing Xiao
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef UTIL_RECT_UNION_H
#define UTIL_RECT_UNION_H

#include <stdbool.h>
#include <pixman.h>
#include <wayland-util.h>

/**
 * @brief Accumulates rectangles and computes a disjoint union on demand.
 * @details This data structure efficiently accumulates a number
 * of rectangles and then, when needed, computes a disjoint cover of their union.
 * (That is: produce a list of disjoint rectangles which covers every point
 * that was contained in one of the added rectangles.)
 *
 * Add rectangles to the union with `rect_union_add()`; to compute the disjoint
 * union, run `rect_union_evaluate()`, which will place the result in `.region`.
 * If there were any allocation failures, `.region` will instead contain the
 * bounding box for the entire list of rectangles.
 *
 * Example usage:
 * @code
 *     struct rect_union r;
 *     rect_union_init(&r);
 *     for (j in ...) {
 *         rect_union_add(&r, box[j]);
 *     }
 *     const pixman_region32_t *reg = rect_union_evaluate(&r);
 *     int nboxes;
 *     pixman_box32_t *boxes = pixman_region32_rectangles(reg, &nboxes);
 *     for (int i = 0; i < nboxes; i++) {
 *         do_stuff(boxes[i]);
 *     }
 *     rect_union_finish(&r);
 * @endcode
 */
struct rect_union {
	pixman_box32_t bounding_box; /**< Bounding box maintained as rectangles are added. */
	pixman_region32_t region; /**< Disjoint union updated by rect_union_evaluate(). */

	struct wl_array unsorted; /**< Array of pending pixman_box32_t rectangles. */
	bool alloc_failure; /**< Whether evaluation must fall back to the bounding box. */
};

/**
 * @brief Initializes an empty rectangle accumulator.
 * @param r Accumulator to initialize; previous contents are disregarded.
 */
void rect_union_init(struct rect_union *r);

/**
 * @brief Releases an initialized rectangle accumulator.
 * @param r Accumulator to finish; becomes invalid until reinitialized.
 */
void rect_union_finish(struct rect_union *r);

/**
 * @brief Adds a nonempty rectangle to the accumulator.
 * @details Amortized time is O(1). Allocation or complexity limits switch accumulation to a
 *          bounding box fallback.
 * @param r Accumulator receiving the rectangle.
 * @param box Rectangle to add; x1 >= x2 or y1 >= y2 is ignored.
 */
void rect_union_add(struct rect_union *r, const pixman_box32_t *box);

/**
 * @brief Evaluates accumulated rectangles into a disjoint region.
 * @details May be interleaved with rect_union_add(). Returns a bounding box if allocation or
 *          complexity limits prevent an exact union. Worst case time is O(t^2) for t
 *          rectangles; disjoint rectangles in y-x band order take O(t).
 * @param r Accumulator to evaluate.
 * @return Internal region, valid until the accumulator is next modified.
 */
const pixman_region32_t *rect_union_evaluate(struct rect_union *r);

#endif
