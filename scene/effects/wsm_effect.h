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
bool wsm_effect_provides_feature (struct wsm_effect *effect,
	enum wsm_effect_feature feature);

#endif
