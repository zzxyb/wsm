/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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
