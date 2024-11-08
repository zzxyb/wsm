#ifndef WSM_PANGO_H
#define WSM_PANGO_H

#include <stdbool.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#ifdef __GNUC__
/**
 * @brief Macro to specify printf-style format attributes for functions.
 * @param start The index of the first variable argument.
 * @param end The index of the last variable argument.
 */
#define _WSM_ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define _WSM_ATTRIB_PRINTF(start, end) // No attributes for non-GNU compilers
#endif

/**
 * @brief Escapes markup text for safe rendering.
 * @param src Pointer to the source string containing markup text.
 * @param dest Pointer to the destination string for the escaped text.
 * @return The length of the escaped text.
 */
size_t escape_markup_text(const char *src, char *dest);

/**
 * @brief Creates a Pango layout for rendering text.
 * @param cairo Pointer to the Cairo context.
 * @param desc Pointer to the Pango font description.
 * @param text Pointer to the text to render.
 * @param scale Scaling factor for the text.
 * @param markup Flag indicating if the text contains markup.
 * @return Pointer to the created PangoLayout.
 */
PangoLayout *get_pango_layout(cairo_t *cairo, const PangoFontDescription *desc,
	const char *text, double scale, bool markup);

/**
 * @brief Gets the size of the rendered text.
 * @param cairo Pointer to the Cairo context.
 * @param desc Pointer to the Pango font description.
 * @param width Pointer to store the width of the text.
 * @param height Pointer to store the height of the text.
 * @param baseline Pointer to store the baseline of the text.
 * @param scale Scaling factor for the text.
 * @param markup Flag indicating if the text contains markup.
 * @param fmt Format string for the text.
 * @param ... Additional arguments for the format string.
 */
void get_text_size(cairo_t *cairo, const PangoFontDescription *desc, int *width, int *height,
	int *baseline, double scale, bool markup, const char *fmt, ...) _WSM_ATTRIB_PRINTF(8, 9);

/**
 * @brief Gets the metrics of the specified font description.
 * @param desc Pointer to the Pango font description.
 * @param height Pointer to store the height of the font.
 * @param baseline Pointer to store the baseline of the font.
 */
void get_text_metrics(const PangoFontDescription *desc, int *height, int *baseline);

/**
 * @brief Renders text using the specified Cairo context and Pango font description.
 * @param cairo Pointer to the Cairo context.
 * @param desc Pointer to the Pango font description.
 * @param scale Scaling factor for the text.
 * @param markup Flag indicating if the text contains markup.
 * @param fmt Format string for the text.
 * @param ... Additional arguments for the format string.
 */
void render_text(cairo_t *cairo, PangoFontDescription *desc,
	double scale, bool markup, const char *fmt, ...) _WSM_ATTRIB_PRINTF(5, 6);

#endif
