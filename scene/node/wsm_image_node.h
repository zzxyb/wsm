#ifndef WSM_IMAGE_NODE_H
#define WSM_IMAGE_NODE_H

#include <stdbool.h>

struct wlr_renderer;
struct wlr_texture;
struct wlr_scene_node;
struct wlr_scene_tree;

typedef struct _cairo_surface cairo_surface_t;

/**
 * @brief Structure representing an image node in the WSM
 */
struct wsm_image_node {
	struct wlr_scene_node *node_wlr; /**< Pointer to the associated WLR scene node */

	int width; /**< Width of the image node */
	int height; /**< Height of the image node */
	float alpha; /**< Alpha (transparency) value of the image node */
};

/**
 * @brief Creates a new wsm_image_node instance
 * @param parent Pointer to the parent scene tree where the image node will be created
 * @param width Width of the image node
 * @param height Height of the image node
 * @param path Path to the image file
 * @param alpha Alpha value for the image node
 * @return Pointer to the newly created wsm_image_node instance
 */
struct wsm_image_node *wsm_image_node_create(struct wlr_scene_tree *parent,
	int width, int height, char *path, float alpha);

/**
 * @brief Updates the alpha value of the specified image node
 * @param node Pointer to the wsm_image_node whose alpha value will be updated
 * @param alpha New alpha value to set
 */
void wsm_image_node_update_alpha(struct wsm_image_node *node, float alpha);

/**
 * @brief Creates a Cairo surface from a file
 * @param file_path Path to the image file
 * @return Pointer to the created Cairo surface, or NULL on failure
 */
cairo_surface_t *create_cairo_surface_frome_file(const char *file_path);

/**
 * @brief Creates a texture from a Cairo surface
 * @param renderer Pointer to the WLR renderer used for creating the texture
 * @param surface Pointer to the Cairo surface from which to create the texture
 * @return Pointer to the created WLR texture
 */
struct wlr_texture *create_texture_from_cairo_surface(struct wlr_renderer *renderer, cairo_surface_t *surface);

/**
 * @brief Creates a texture from a file
 * @param renderer Pointer to the WLR renderer used for creating the texture
 * @param file_path Path to the image file
 * @return Pointer to the created WLR texture
 */
struct wlr_texture *create_texture_from_file_v1(struct wlr_renderer *renderer, const char *file_path);

/**
 * @brief Loads an image file into the specified image node
 * @param node Pointer to the wsm_image_node where the image will be loaded
 * @param file_path Path to the image file to be loaded
 */
void wsm_image_node_load(struct wsm_image_node *node, const char *file_path);

/**
 * @brief Sets the size of the specified image node
 * @param node Pointer to the wsm_image_node whose size will be set
 * @param width New width for the image node
 * @param height New height for the image node
 */
void wsm_image_node_set_size(struct wsm_image_node *node, int width, int height);

#endif
