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

#include "wsm_font.h"
#include "wsm_log.h"
#include "wsm_pango.h"

#include <assert.h>

struct wsm_font *wsm_font_create() {
    struct wsm_font *font = calloc(1, sizeof(struct wsm_font));
    if (!wsm_assert(font, "Could not create wsm_font: allocation failed!")) {
        return NULL;
    }

    // TODO(YaoBing Xiao): temporarily fixed value
    font->font = "Noto Sans 10";
    PangoFontDescription *font_description = pango_font_description_from_string(font->font);

    const char *family = pango_font_description_get_family(font_description);
    if (family == NULL) {
        pango_font_description_free(font_description);
        wsm_log(WSM_ERROR, "Invalid font family.");
        return NULL;
    }

    const gint size = pango_font_description_get_size(font_description);
    if (size == 0) {
        pango_font_description_free(font_description);
        wsm_log(WSM_ERROR, "Invalid font size.");
    }

    font->font_description = font_description;
    update_font_height(font);
    return font;
}

void update_font_height(struct wsm_font *font) {
    get_text_metrics(font->font_description, &font->font_height, &font->font_baseline);
}

void wsm_font_destroy(struct wsm_font *font) {
    if (!font) {
        wsm_log(WSM_ERROR, "wsm_font is NULL!");
        return;
    }

    if (font->font_description) {
        pango_font_description_free(font->font_description);
    }
    free(font);
}
