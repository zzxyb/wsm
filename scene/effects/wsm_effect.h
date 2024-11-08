#ifndef WSM_EFFECT_H
#define WSM_EFFECT_H

#include <stdbool.h>

/**
 * @brief Enumeration of built-in effects features
 */
enum wsm_effect_feature {
	WSM_EFFECT_NOTHING = 0, /**< No build-id effect. */
	WSM_EFFECT_BLUR,        /**< Support for window blur effect. */
	WSM_EFFECT_BORDER,      /**< Support for window border effect. */
	WSM_EFFECT_RADIUS,      /**< Support for scissors window radius effect. */
};

/**
 * @brief Structure representing the implementation of effects
 */
struct wsm_effect_impl {
	/**
	 * @brief Called before starting to render the screen.
	 */
	void (*pre_render_output) ();

	/**
	 * @brief Renders something on top of the windows.
	 */
	void (*render_output) ();
	
	/**
	 * @brief Called after all rendering has been finished.
	 */
	void (*post_render_output) ();
	
	/**
	 * @brief Called for every window before the actual render pass.
	 */
	void (*pre_render_window) ();
	
	/**
	 * @brief Performs various transformations on the window.
	 * Changes opacity, brightness, and saturation of the window.
	 */
	void (*render_window) ();
	
	/**
	 * @brief Called for every window after all rendering has been finished.
	 */
	void (*post_render_window) ();
	
	/**
	 * @brief Checks whether this special effect is supported,
	 * distinguishing between different rendering APIs.
	 */
	bool (*is_supported) ();
};

/**
 * @brief Structure representing an effect in the WSM
 */
struct wsm_effect {
	const struct wsm_effect_impl *impl; /**< Pointer to the effect implementation */
};

/**
 * @brief Creates a new wsm_effect instance
 * @return Pointer to the newly created wsm_effect instance
 */
struct wsm_effect* wsm_effect_create();

/**
 * @brief Destroys the specified wsm_effect instance
 * @param effect Pointer to the wsm_effect instance to be destroyed
 */
void wsm_effect_destroy(struct wsm_effect* effect);

/**
 * @brief Checks if the specified effect provides a certain feature
 * @param effect Pointer to the wsm_effect to be checked
 * @param feature The feature to check for
 * @return true if the feature is provided, false otherwise
 */
bool wsm_effect_provides_feature (struct wsm_effect *effect,
	enum wsm_effect_feature feature);

#endif
