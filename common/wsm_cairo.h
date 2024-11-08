#ifndef WSM_CAIRO_H
#define WSM_CAIRO_H

#include <stdint.h>

#include <cairo.h>

#include <wayland-client-protocol.h>

/**
 * @brief Sets the source color for a Cairo context using a 32-bit unsigned integer.
 * @param cairo Pointer to the Cairo context.
 * @param color The color value in ARGB format.
 */
void cairo_set_source_u32(cairo_t *cairo, uint32_t color);

/**
 * @brief Converts a Wayland subpixel enumeration to a Cairo subpixel order.
 * @param subpixel The Wayland subpixel enumeration to convert.
 * @return The corresponding Cairo subpixel order.
 */
cairo_subpixel_order_t to_cairo_subpixel_order(enum wl_output_subpixel subpixel);

/**
 * @brief Scales a Cairo image surface to the specified width and height.
 * @param image Pointer to the Cairo surface to scale.
 * @param width The new width for the scaled surface.
 * @param height The new height for the scaled surface.
 * @return Pointer to the newly scaled Cairo surface.
 */
cairo_surface_t *cairo_image_surface_scale(cairo_surface_t *image,
	int width, int height);

#endif
