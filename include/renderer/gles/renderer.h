/**
 * @file        renderer.h
 * @brief       GLES2 renderer state and utility functions.
 * @details     This file defines the GLES2 renderer object, its extension and
 *              shader state, and helpers for renderer and shader management.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef RENDERER_GLES_RENDERER_H
#define RENDERER_GLES_RENDERER_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <wlr/render/drm_format_set.h>
#include <wlr/render/wlr_renderer.h>

typedef void (GL_APIENTRYP PFNGLGETINTEGER64VEXTPROC) (GLenum pname, GLint64 *data);

struct wsm_gles2_buffer;
struct wsm_egl;

/**
 * @brief Uniform and attribute locations for a GLES2 texture shader.
 */
struct wsm_gles2_tex_shader {
	GLuint program; /**< Linked GLES shader program. */
	GLint proj; /**< Projection matrix uniform location. */
	GLint tex_proj; /**< Texture projection uniform location. */
	GLint tex; /**< Texture sampler uniform location. */
	GLint alpha; /**< Alpha uniform location. */
	GLint pos_attrib; /**< Position attribute location. */
};

/**
 * @brief GLES2 renderer state and backend resources.
 */
struct wsm_gles2_renderer {
	struct wlr_renderer base; /**< Generic wlroots renderer interface. */

	struct wsm_egl *egl; /**< EGL state used by the renderer. */
	int drm_fd; /**< DRM file descriptor used for device resources. */

	struct wlr_drm_format_set shm_texture_formats; /**< Supported shared-memory formats. */

	const char *exts_str; /**< OpenGL extension string. */
	struct {
		bool EXT_read_format_bgra; /**< BGRA readback support. */
		bool KHR_debug; /**< GLES debug output support. */
		bool OES_egl_image_external; /**< External EGL image sampling support. */
		bool OES_egl_image; /**< EGL image support. */
		bool EXT_texture_type_2_10_10_10_REV; /**< Packed 10:10:10:2 support. */
		bool OES_texture_half_float_linear; /**< Linear half-float filtering support. */
		bool EXT_texture_norm16; /**< Normalized 16-bit texture support. */
		bool EXT_disjoint_timer_query; /**< GPU timer query support. */
	} exts;

	struct {
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES; /**< Binds an EGL image to a texture. */
		PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR; /**< Registers a debug callback. */
		PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControlKHR; /**< Controls debug messages. */
		PFNGLPOPDEBUGGROUPKHRPROC glPopDebugGroupKHR; /**< Pops a debug group. */
		PFNGLPUSHDEBUGGROUPKHRPROC glPushDebugGroupKHR; /**< Pushes a debug group. */
		PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES; /**< Binds an EGL image to a renderbuffer. */
		PFNGLGETGRAPHICSRESETSTATUSKHRPROC glGetGraphicsResetStatusKHR; /**< Queries graphics reset status. */
		PFNGLGENQUERIESEXTPROC glGenQueriesEXT; /**< Creates timer queries. */
		PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT; /**< Deletes timer queries. */
		PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT; /**< Records a GPU timestamp. */
		PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT; /**< Queries an integer result. */
		PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT; /**< Queries a 64-bit result. */
		PFNGLGETINTEGER64VEXTPROC glGetInteger64vEXT; /**< Queries a 64-bit integer value. */
	} procs;

	struct {
		struct {
			GLuint program; /**< Linked solid-color program. */
			GLint proj; /**< Projection matrix uniform location. */
			GLint color; /**< Color uniform location. */
			GLint pos_attrib; /**< Position attribute location. */
		} quad;
		struct wsm_gles2_tex_shader tex_rgba;
		struct wsm_gles2_tex_shader tex_rgbx;
		struct wsm_gles2_tex_shader tex_ext;
	} shaders;

	struct wl_list buffers; /**< List of wsm_gles2_buffer.link entries. */
	struct wl_list textures; /**< List of wsm_gles2_texture.link entries. */
};

/**
 * @brief Creates a GLES2 renderer for a DRM device.
 * @param drm_fd DRM device file descriptor.
 * @return GLES2 renderer, or NULL on failure.
 */
struct wsm_gles2_renderer *wsm_gles2_renderer_create_with_drm_fd(int drm_fd);

/**
 * @brief Creates a GLES2 renderer using existing EGL state.
 * @param egl EGL state used by the renderer.
 * @return GLES2 renderer, or NULL on failure.
 */
struct wsm_gles2_renderer *wsm_gles2_renderer_create(struct wsm_egl *egl);

/**
 * @brief Checks whether a renderer is a wsm GLES2 renderer.
 * @param wlr_renderer Renderer to inspect.
 * @return true when the renderer is GLES2-backed, false otherwise.
 */
bool wlr_renderer_is_wsm_gles2(const struct wlr_renderer *wlr_renderer);

/**
 * @brief Casts a generic renderer to a GLES2 renderer.
 * @param wlr_renderer Renderer known to be GLES2-backed.
 * @return Enclosing GLES2 renderer, or NULL when the type does not match.
 */
struct wsm_gles2_renderer *wsm_gles2_renderer_from_renderer(
	struct wlr_renderer *wlr_renderer);

/**
 * @brief Pushes a GLES debug group with source information.
 * @param renderer GLES2 renderer receiving the debug group.
 * @param file Source file name.
 * @param func Source function name.
 */
void wsm_gles2_push_debug_(struct wsm_gles2_renderer *renderer,
	const char *file, const char *func);

/** @brief Pushes a debug group using the current source location. */
#define wsm_gles2_push_debug(renderer) wsm_gles2_push_debug_(renderer, _WSM_FILENAME, __func__)

/**
 * @brief Pops the current GLES debug group.
 * @param renderer GLES2 renderer whose debug group is popped.
 */
void wsm_gles2_pop_debug(struct wsm_gles2_renderer *renderer);

/**
 * @brief Links a GLES shader program.
 * @param renderer GLES2 renderer used for linking.
 * @param vert_src Vertex shader source.
 * @param frag_src Fragment shader source.
 * @return Linked program name, or zero on failure.
 */
GLuint wsm_gles2_link_program(struct wsm_gles2_renderer *renderer,
	const GLchar *vert_src, const GLchar *frag_src);

/**
 * @brief Uploads a projection matrix and box to a GLES uniform.
 * @param loc Projection uniform location.
 * @param proj Projection matrix storage.
 * @param box Box to project.
 */
void wsm_gles_set_proj_matrix(GLint loc, float proj[9], const struct wlr_box *box);

#endif // RENDERER_GLES_RENDERER_H
