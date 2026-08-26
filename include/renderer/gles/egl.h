/**
 * @file        egl.h
 * @brief       EGL context management for the wsm GLES renderer.
 * @details     This file defines the EGL state, extension table, and helper
 *              functions used for DMA-BUF import and synchronization.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_EGL_H
#define RENDERER_EGL_H

#include <stdbool.h>

#include <pixman.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wlr/render/drm_format_set.h>
#include <wlr/render/dmabuf.h>

/**
 * @brief EGL state used by the wsm GLES renderer.
 */
struct wsm_egl {
	EGLDisplay display; /**< EGL display connection. */
	EGLContext context; /**< EGL rendering context. */
	EGLDeviceEXT device; /**< EGL device, or EGL_NO_DEVICE_EXT. */
	struct gbm_device *gbm_device; /**< GBM device used by the display. */

	struct {
		bool KHR_image_base; /**< EGL_KHR_image_base support. */
		bool EXT_image_dma_buf_import; /**< EGL_EXT_image_dma_buf_import support. */
		bool EXT_image_dma_buf_import_modifiers; /**< Modifier import support. */
		bool IMG_context_priority; /**< EGL_IMG_context_priority support. */
		bool EXT_create_context_robustness; /**< Robust context support. */

		bool EXT_device_drm; /**< EGL_EXT_device_drm support. */
		bool EXT_device_drm_render_node; /**< Render-node query support. */

		bool EXT_device_query; /**< EGL_EXT_device_query support. */
		bool KHR_platform_gbm; /**< EGL_KHR_platform_gbm support. */
		bool EXT_platform_device; /**< EGL_EXT_platform_device support. */
		bool KHR_display_reference; /**< EGL_KHR_display_reference support. */
	} exts;

	struct {
		PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT; /**< Gets a platform display. */
		PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR; /**< Creates an EGL image. */
		PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR; /**< Destroys an EGL image. */
		PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT; /**< Queries DMA-BUF formats. */
		PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT; /**< Queries DMA-BUF modifiers. */
		PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR; /**< Controls EGL debug messages. */
		PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT; /**< Queries display attributes. */
		PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT; /**< Queries an EGL device string. */
		PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT; /**< Enumerates EGL devices. */
		PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR; /**< Creates an EGL sync object. */
		PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR; /**< Destroys an EGL sync object. */
		PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID; /**< Duplicates a native fence FD. */
		PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR; /**< Waits for an EGL sync object. */
	} procs;

	bool has_modifiers; /**< Whether DMA-BUF modifiers are supported. */
	struct wlr_drm_format_set dmabuf_texture_formats; /**< Formats usable for sampling. */
	struct wlr_drm_format_set dmabuf_render_formats; /**< Formats usable for rendering. */
};

/**
 * @brief Saved EGL context and surface state.
 */
struct wsm_egl_context {
	EGLDisplay display; /**< Saved EGL display. */
	EGLContext context; /**< Saved EGL context. */
	EGLSurface draw_surface; /**< Saved draw surface. */
	EGLSurface read_surface; /**< Saved read surface. */
};

/**
 * @brief Initializes an EGL context for a DRM file descriptor.
 * @param drm_fd DRM device file descriptor.
 * @return Newly allocated EGL state, or NULL on failure.
 */
struct wsm_egl *wsm_egl_create_with_drm_fd(int drm_fd);

/**
 * @brief Frees EGL resources and releases the current context.
 * @param egl EGL state to destroy.
 */
void wsm_egl_destroy(struct wsm_egl *egl);

/**
 * @brief Creates an EGL image from DMA-BUF attributes.
 * @param egl EGL state used to create the image.
 * @param attributes DMA-BUF attributes to import.
 * @param external_only Destination for whether the image requires external sampling.
 * @return Created EGL image, or EGL_NO_IMAGE_KHR on failure.
 */
EGLImageKHR wsm_egl_create_image_from_dmabuf(struct wsm_egl *egl,
	struct wlr_dmabuf_attributes *attributes, bool *external_only);

/**
 * @brief Gets DMA-BUF formats suitable for sampling.
 * @param egl EGL state to inspect.
 * @return Renderer-owned texture format set.
 */
const struct wlr_drm_format_set *wsm_egl_get_dmabuf_texture_formats(struct wsm_egl *egl);

/**
 * @brief Gets DMA-BUF formats suitable for rendering.
 * @param egl EGL state to inspect.
 * @return Renderer-owned render format set.
 */
const struct wlr_drm_format_set *wsm_egl_get_dmabuf_render_formats(struct wsm_egl *egl);

/**
 * @brief Destroys an EGL image created by an EGL state.
 * @param egl EGL state that owns the image.
 * @param image EGL image to destroy.
 * @return true on success, false when the image could not be destroyed.
 */
bool wsm_egl_destroy_image(struct wsm_egl *egl, EGLImageKHR image);

/**
 * @brief Duplicates the DRM file descriptor associated with an EGL state.
 * @param egl EGL state to inspect.
 * @return Duplicated file descriptor, or -1 on failure.
 */
int wsm_egl_dup_drm_fd(struct wsm_egl *egl);

/**
 * @brief Restores a previously saved EGL context.
 * @param context Saved context state.
 * @return true on success, false on failure.
 */
bool wsm_egl_restore_context(struct wsm_egl_context *context);

/**
 * @brief Makes an EGL context current and saves the previous context.
 * @param egl EGL state to make current.
 * @param save_context Destination for the previous context state.
 * @return true on success, false on failure.
 */
bool wsm_egl_make_current(struct wsm_egl *egl, struct wsm_egl_context *save_context);

/**
 * @brief Clears the current EGL context.
 * @param egl EGL state whose context is current.
 * @return true on success, false on failure.
 */
bool wsm_egl_unset_current(struct wsm_egl *egl);

/**
 * @brief Creates an EGL synchronization object from a fence file descriptor.
 * @param egl EGL state used to create the sync object.
 * @param fence_fd Fence file descriptor, or -1 when no native fence is supplied.
 * @return Created EGL sync object, or EGL_NO_SYNC_KHR on failure.
 */
EGLSyncKHR wsm_egl_create_sync(struct wsm_egl *egl, int fence_fd);

/**
 * @brief Destroys an EGL synchronization object.
 * @param egl EGL state that owns the sync object.
 * @param sync Sync object to destroy.
 */
void wsm_egl_destroy_sync(struct wsm_egl *egl, EGLSyncKHR sync);

/**
 * @brief Duplicates the native fence file descriptor from an EGL sync object.
 * @param egl EGL state that owns the sync object.
 * @param sync Sync object to inspect.
 * @return Duplicated fence file descriptor, or -1 on failure.
 */
int wsm_egl_dup_fence_fd(struct wsm_egl *egl, EGLSyncKHR sync);

/**
 * @brief Waits for an EGL synchronization object on the server.
 * @param egl EGL state used for the wait.
 * @param sync Sync object to wait for.
 * @return true on success, false on failure.
 */
bool wsm_egl_wait_sync(struct wsm_egl *egl, EGLSyncKHR sync);

/**
 * @brief Wraps an existing EGL display and context.
 * @param display EGL display to wrap.
 * @param context EGL context to wrap.
 * @return Newly allocated EGL state, or NULL on failure.
 */
struct wsm_egl *wsm_egl_create_with_context(EGLDisplay display, EGLContext context);

#endif // RENDERER_EGL_H
