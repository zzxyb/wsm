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

#ifndef WSM_EFFECT_H
#define WSM_EFFECT_H

#include <stdbool.h>

/**
 * @brief build-in effects enumeration
 */
enum wsm_effect_feature {
	WSM_EFFECT_NOTHING = 0, /**< No build-id effect. */
	WSM_EFFECT_BLUR,        /**< supprot window blur effect. */
	WSM_EFFECT_BORDER,      /**< supprot window border effect. */
	WSM_EFFECT_RADIUS,      /**< supprot scissors window radius effect. */
};

struct wsm_effect_impl {
	/**
	 * @brief Called before starting to render the screen.
	 */
	void (*pre_render_output) ();
	/**
	 * @brief render something on top of the windows.
	 */
	void (*render_output) ();
	
	/**
	 * @brief Called after all the render has been finished.
	 */
	void (*post_render_output) ();
	
	/**
	 * @brief Called for every window before the actual render pass.
	 */
	void (*pre_render_window) ();
	
	/**
	 * @brief do various transformations.
	 * change opacity、brightness、saturation of the window
	 */
	void (*render_window) ();
	
	/**
	 * @brief Called for every window after all rendering has been finished.
	 */
	void (*post_render_window) ();
	
	/**
	 * @brief Whether to support this special effect, distinguish different rendering APIs.
	 */
	bool (*is_supported) ();
};

struct wsm_effect {
	const struct wsm_effect_impl *impl;
};

struct wsm_effect* wsm_effect_create();
void wsm_effect_destroy(struct wsm_effect* effect);
bool wsm_effect_provides_feature (struct wsm_effect *effect, enum wsm_effect_feature feature);

#endif
