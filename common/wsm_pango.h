#ifndef WSM_PANGO_H
#define WSM_PANGO_H

#include <stdbool.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#ifdef __GNUC__
#define _WSM_ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define _WSM_ATTRIB_PRINTF(start, end)
#endif

size_t escape_markup_text(const char *src, char *dest);
PangoLayout *get_pango_layout(cairo_t *cairo, const PangoFontDescription *desc,
	const char *text, double scale, bool markup);
void get_text_size(cairo_t *cairo, const PangoFontDescription *desc, int *width, int *height,
	int *baseline, double scale, bool markup, const char *fmt, ...) _WSM_ATTRIB_PRINTF(8, 9);
void get_text_metrics(const PangoFontDescription *desc, int *height, int *baseline);
void render_text(cairo_t *cairo, PangoFontDescription *desc,
	double scale, bool markup, const char *fmt, ...) _WSM_ATTRIB_PRINTF(5, 6);

#endif
