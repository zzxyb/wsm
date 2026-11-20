/* SPDX-License-Identifier: MIT */

/**
 * @file        renderer.h
 * @brief       Vulkan renderer state and resource management.
 * @details     Defines rendering pipelines, command buffers, staging allocations, color transforms,
 *              and synchronization helpers.
 *
 * @par Upstream:
 *              Derived from wlroots; see LICENSE.wlroots in the project root.
 *
 * @author      YaoBing Xiao (wsm adaptations)
 * @date        2026-11-19
 * @version     v1.0
 * @par Copyright(c):
 *              Upstream copyright notices and MIT license: LICENSE.wlroots
 *              in the project root. See LICENSE for wsm copyright and license.
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-11-19, initial version\n
 */

#ifndef RENDER_VULKAN_H
#define RENDER_VULKAN_H

#include "util/wsm_log.h"
#include "renderer/vulkan/device.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/color.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/interface.h>
#include <wlr/util/addon.h>

struct wsm_vk_descriptor_pool;
struct wsm_vk_texture;
struct wsm_vk_render_pass;

/** @brief Image usage masks for render targets, uploads, and imported textures. */
extern const VkImageUsageFlags vulkan_render_usage, vulkan_shm_tex_usage, vulkan_dma_tex_usage;

/**
 * @brief Sampler and YCbCr conversion settings identifying a pipeline layout.
 */
struct wsm_vk_pipeline_layout_key {
	enum wlr_scale_filter_mode filter_mode; /**< Texture sampling filter mode. */

	struct {
		const struct wsm_vk_format *format; /**< Pixel format mapping used by the image. */
		enum wlr_color_encoding encoding; /**< YCbCr color encoding used for sampler conversion. */
		enum wlr_color_range range; /**< YCbCr color range used for sampler conversion. */
	} ycbcr; /**< Additional conversion state used only by YCbCr pipelines. */
};

/**
 * @brief Cached pipeline layout, descriptor layout, and immutable sampler.
 */
struct wsm_vk_pipeline_layout {
	struct wsm_vk_pipeline_layout_key key; /**< Configuration key used to identify the cached object. */

	VkPipelineLayout vk; /**< Vulkan pipeline layout handle. */
	VkDescriptorSetLayout ds; /**< Descriptor set layout for texture sampling. */
	VkSampler sampler; /**< Immutable sampler used by this pipeline layout. */

	struct {
		VkSamplerYcbcrConversion conversion; /**< YCbCr sampler conversion handle. */
		VkFormat format; /**< Vulkan format used for YCbCr conversion. */
	} ycbcr; /**< Additional conversion state used only by YCbCr pipelines. */

	struct wl_list link; /**< Link in wsm_vk_renderer.pipeline_layouts. */
};

/**
 * @brief Texture transfer function selectors matching shaders/texture.frag.
 */
enum wsm_vk_texture_transform {
	WLR_VK_TEXTURE_TRANSFORM_IDENTITY = 0, /**< Leaves sampled color values unchanged. */
	WLR_VK_TEXTURE_TRANSFORM_SRGB = 1, /**< Decodes the sRGB transfer function. */
	WLR_VK_TEXTURE_TRANSFORM_ST2084_PQ = 2, /**< Decodes the ST 2084 PQ transfer function. */
	WLR_VK_TEXTURE_TRANSFORM_GAMMA22 = 3, /**< Decodes a gamma 2.2 transfer function. */
	WLR_VK_TEXTURE_TRANSFORM_BT1886 = 4, /**< Decodes the BT.1886 transfer function. */
};

/**
 * @brief Fragment shader source used by a graphics pipeline.
 */
enum wsm_vk_shader_source {
	WLR_VK_SHADER_SOURCE_TEXTURE, /**< Samples a texture in the fragment shader. */
	WLR_VK_SHADER_SOURCE_ROUNDED_RECT, /**< Computes rounded rectangle coverage in the fragment shader. */
	WSM_VK_SHADER_SOURCE_ROUNDED_TEXTURE, /**< Samples a texture and clips it to rounded destination corners. */
	WSM_VK_SHADER_SOURCE_BORDER, /**< Computes superellipse border coverage in the fragment shader. */
	WSM_VK_SHADER_SOURCE_SHADOW, /**< Computes shadow coverage in the fragment shader. */
	WSM_VK_SHADER_SOURCE_BLUR_DOWN, /**< Downsamples a Dual Kawase blur level. */
	WSM_VK_SHADER_SOURCE_BLUR_UP, /**< Upsamples a Dual Kawase blur level. */
	WSM_VK_SHADER_SOURCE_BLUR_FINISH, /**< Draws the final blur result. */
	WLR_VK_SHADER_SOURCE_SINGLE_COLOR, /**< Uses a uniform solid color in the fragment shader. */
};

/**
 * @brief Output transfer function selectors matching shaders/output.frag.
 */
enum wsm_vk_output_transform {
	WLR_VK_OUTPUT_TRANSFORM_IDENTITY = 0, /**< Leaves blended color values unchanged. */
	WLR_VK_OUTPUT_TRANSFORM_INVERSE_SRGB = 1, /**< Encodes the output with the inverse sRGB function. */
	WLR_VK_OUTPUT_TRANSFORM_INVERSE_ST2084_PQ = 2, /**< Encodes the output with the inverse ST 2084 PQ function. */
	WLR_VK_OUTPUT_TRANSFORM_LUT3D = 3, /**< Applies the full three-dimensional color transform table. */
	WLR_VK_OUTPUT_TRANSFORM_INVERSE_GAMMA22 = 4, /**< Encodes the output with an inverse gamma 2.2 function. */
	WLR_VK_OUTPUT_TRANSFORM_INVERSE_BT1886 = 5, /**< Encodes the output with the inverse BT.1886 function. */
};

/**
 * @brief Shader, blending, and layout settings identifying a graphics pipeline.
 */
struct wsm_vk_pipeline_key {
	struct wsm_vk_pipeline_layout_key layout; /**< Sampler and YCbCr conversion configuration. */
	enum wsm_vk_shader_source source; /**< Fragment shader source selected for the pipeline. */
	enum wlr_render_blend_mode blend_mode; /**< Blend mode used by the graphics pipeline. */

	enum wsm_vk_texture_transform texture_transform; /**< Transfer function applied when the source is a texture. */
};

/**
 * @brief Graphics pipeline cached within a render format setup.
 */
struct wsm_vk_pipeline {
	struct wsm_vk_pipeline_key key; /**< Configuration key used to identify the cached object. */

	VkPipeline vk; /**< Vulkan graphics pipeline handle. */
	const struct wsm_vk_pipeline_layout *layout; /**< Cached pipeline layout used by this pipeline. */
	struct wsm_vk_render_format_setup *setup; /**< Render format setup owning this pipeline. */
	struct wl_list link; /**< Link in wsm_vk_render_format_setup.pipelines. */
};

/**
 * @brief Render passes and pipelines for one output format configuration.
 */
struct wsm_vk_render_format_setup {
	struct wl_list link; /**< Link in wsm_vk_renderer.render_format_setups. */
	const struct wsm_vk_format *render_format; /**< Output image format mapping used by the render passes. */
	bool use_blending_buffer; /**< Whether a separate linear-light blending attachment is used. */
	bool use_srgb; /**< Whether the output attachment uses an sRGB view. */
	VkRenderPass render_pass; /**< Render pass preserving the existing attachment contents. */
	VkRenderPass render_pass_clear; /**< Render pass clearing the attachment before drawing. */

	VkPipeline output_pipe_identity; /**< Output pipeline applying no transfer function. */
	VkPipeline output_pipe_srgb; /**< Output pipeline applying the inverse sRGB transfer function. */
	VkPipeline output_pipe_pq; /**< Output pipeline applying the inverse ST 2084 PQ transfer function. */
	VkPipeline output_pipe_lut3d; /**< Output pipeline applying a three-dimensional color lookup table. */
	VkPipeline output_pipe_gamma22; /**< Output pipeline applying the inverse gamma 2.2 transfer function. */
	VkPipeline output_pipe_bt1886; /**< Output pipeline applying the inverse BT.1886 transfer function. */

	struct wsm_vk_renderer *renderer; /**< Renderer owning these resources. */
	struct wl_list pipelines; /**< List of wsm_vk_pipeline.link entries. */
};

/**
 * @brief Output image view, framebuffer, and layout transition state.
 */
struct wsm_vk_render_buffer_out {
	VkImageView image_view; /**< Vulkan image view handle. */
	VkFramebuffer framebuffer; /**< Framebuffer containing the output attachment. */
	bool transitioned; /**< Whether the image has left its initial layout. */
};

/**
 * @brief Reusable command buffer and resources retained until GPU completion.
 */
struct wsm_vk_command_buffer {
	VkCommandBuffer vk; /**< Vulkan command buffer handle. */
	bool recording; /**< Whether the command buffer is currently being recorded. */
	uint64_t timeline_point; /**< Renderer timeline point associated with completion. */
	struct wl_list destroy_textures; /**< List of wsm_vk_texture.destroy_link entries pending completion. */
	struct wlr_color_transform *color_transform; /**< Color transform retained for rendering; may be NULL. */

	VkSemaphore binary_semaphore; /**< Optional binary semaphore for sync-file release; may be VK_NULL_HANDLE. */

	struct wl_array wait_semaphores; /**< Array of VkSemaphore handles used for imported acquisition fences. */
};

/** @brief Maximum number of command buffers retained by a renderer. */
#define VULKAN_COMMAND_BUFFERS_CAP 64

/**
 * @brief Vulkan renderer resources, caches, and submission state.
 */
struct wsm_vk_renderer {
	struct wlr_renderer wlr_renderer; /**< Generic wlroots renderer interface. */
	struct wlr_backend *backend; /**< Associated wlroots backend; may be NULL. */
	struct wsm_vk_device *dev; /**< Vulkan device state owned by the renderer. */

	VkCommandPool command_pool; /**< Pool allocating rendering and staging command buffers. */

	VkShaderModule vert_module; /**< Common vertex shader module. */
	VkShaderModule round_tex_frag_module; /**< Rounded texture fragment shader module. */
	VkShaderModule tex_frag_module; /**< Texture fragment shader module. */
	VkShaderModule blur_down_frag_module; /**< Blur downsampling fragment shader module. */
	VkShaderModule blur_up_frag_module; /**< Blur upsampling fragment shader module. */
	VkShaderModule blur_finish_frag_module; /**< Final blur fragment shader module. */
	VkShaderModule shadow_frag_module; /**< Shadow fragment shader module. */
	VkShaderModule rect_frag_module; /**< Rounded rectangle fragment shader module. */
	VkShaderModule border_frag_module; /**< Superellipse border fragment shader module. */
	VkShaderModule quad_frag_module; /**< Solid-color fragment shader module. */
	VkShaderModule output_module; /**< Blending-to-output fragment shader module. */

	struct wl_list pipeline_layouts; /**< List of wsm_vk_pipeline_layout.link entries. */

	VkPipelineLayout output_pipe_layout; /**< Pipeline layout for the blending-to-output subpass. */
	VkDescriptorSetLayout output_ds_srgb_layout; /**< Descriptor layout sampling the blending attachment. */
	VkDescriptorSetLayout output_ds_lut3d_layout; /**< Descriptor layout sampling a color lookup table. */
	VkSampler output_sampler_lut3d; /**< Sampler used for three-dimensional color lookup tables. */
	VkDescriptorSet output_ds_lut3d_dummy; /**< Descriptor set for the dummy one-texel lookup table. */
	struct wsm_vk_descriptor_pool *output_ds_lut3d_dummy_pool; /**< Pool owning the dummy lookup table descriptor set. */

	size_t last_output_pool_size; /**< Most recent output descriptor pool allocation size. */
	struct wl_list output_descriptor_pools; /**< List of output wsm_vk_descriptor_pool.link entries. */

	VkImage dummy3d_image; /**< Dummy three-dimensional image for shaders without a lookup table. */
	VkDeviceMemory dummy3d_mem; /**< Memory backing the dummy lookup table image. */
	VkImageView dummy3d_image_view; /**< View of the dummy lookup table image. */
	bool dummy3d_image_transitioned; /**< Whether the dummy image has left its initial layout. */

	VkSemaphore timeline_semaphore; /**< Timeline semaphore tracking renderer submission completion. */
	uint64_t timeline_point; /**< Renderer timeline point associated with completion. */

	size_t last_pool_size; /**< Most recent texture descriptor pool allocation size. */
	struct wl_list descriptor_pools; /**< List of texture wsm_vk_descriptor_pool.link entries. */
	struct wl_list render_format_setups; /**< List of wsm_vk_render_format_setup.link entries. */


	struct wl_list textures; /**< List of wsm_vk_texture.link entries. */
	struct wl_list foreign_textures; /**< List of imported textures to return to their foreign queue family. */

	struct wl_list render_buffers; /**< List of wsm_vk_render_buffer.link entries. */

	struct wl_list color_transforms; /**< List of wsm_vk_color_transform.link entries. */

	struct {
		struct wsm_vk_texture *levels[9]; /**< Cached Dual Kawase pyramid. */
		struct wsm_vk_texture *filtered; /**< Appearance-adjusted texture. */
	} blur; /**< Cached render targets for Dual Kawase blur and appearance adjustment. */

	struct wsm_vk_command_buffer command_buffers[VULKAN_COMMAND_BUFFERS_CAP]; /**< Reusable command buffers retained by the renderer. */

	struct {
		struct wsm_vk_command_buffer *cb; /**< Current staging command buffer; may be NULL. */
		uint64_t last_timeline_point; /**< Timeline point of the most recent staging submission. */
		struct wl_list buffers; /**< List of wsm_vk_stage_buffer.link entries. */
	} stage; /**< Staging transfer recording and allocation state. */

	struct {
		bool initialized; /**< Whether the readback cache contains an allocated image. */
		uint32_t drm_format; /**< DRM fourcc format identifier. */
		uint32_t width, height; /**< Cached image width and height in pixels. */
		VkImage dst_image; /**< Cached destination image for CPU readback. */
		VkDeviceMemory dst_img_memory; /**< Host-visible memory backing the readback image. */
	} read_pixels_cache; /**< Cached image and memory used for pixel readback. */
};

/**
 * @brief Vertex shader push constants for projection and texture coordinates.
 */
struct wsm_vk_vert_pcr_data {
	float proj_packed[6]; /**< Packed two-dimensional projection matrix coefficients. */
	float uv_off[2]; /**< Texture coordinate origin. */
	float uv_size[2]; /**< Texture coordinate scale. */
	float padding[2]; /**< Padding preserving the shader push constant layout. */
};

/**
 * @brief Texture fragment shader color conversion push constants.
 */
struct wsm_vk_frag_texture_pcr_data {
	float matrix[4][4]; /**< Color conversion matrix; only a 3 by 3 subset is used. */
	float alpha; /**< Opacity multiplier applied to the sampled color. */
	float luminance_multiplier; /**< Luminance scale applied during texture color conversion. */
};

/**
 * @brief Rounded texture color conversion and destination-space corner constants.
 */
struct wsm_vk_frag_round_texture_pcr_data {
	float matrix[9]; /**< Packed row-major color conversion matrix. */
	float alpha; /**< Opacity multiplier applied to the sampled color. */
	float luminance_multiplier; /**< Luminance scale applied during texture color conversion. */
	float padding; /**< Aligns the corner radii with the shader push constant layout. */
	float radii[4]; /**< Destination-space corner radii. */
	float box[4]; /**< Destination x, y, width, and height in target pixels. */
};

/**
 * @brief Output fragment shader color conversion push constants.
 */
struct wsm_vk_frag_output_pcr_data {
	float matrix[4][4]; /**< Color conversion matrix; only a 3 by 3 subset is used. */
	float lut_3d_offset; /**< Lookup coordinate offset for texel center alignment. */
	float lut_3d_scale; /**< Lookup coordinate scale for texel center alignment. */
};

/**
 * @brief Cached image view and descriptor set for a texture pipeline layout.
 */
struct wsm_vk_texture_view {
	struct wl_list link; /**< Link in wsm_vk_texture.views. */
	const struct wsm_vk_pipeline_layout *layout; /**< Pipeline layout compatible with this view. */
	bool srgb; /**< Whether the image view uses the sRGB format variant. */

	VkDescriptorSet ds; /**< Vulkan descriptor set handle. */
	VkImageView image_view; /**< Vulkan image view handle. */
	struct wsm_vk_descriptor_pool *ds_pool; /**< Pool owning the descriptor set. */
};

/**
 * @brief Creates a Vulkan renderer for a DRM device.
 * @param drm_fd DRM device file descriptor.
 * @return Renderer, or NULL on failure.
 */
struct wlr_renderer *wsm_vk_renderer_create_with_drm_fd(int drm_fd);

/**
 * @brief Gets or creates a graphics pipeline for a render format.
 * @param setup Render format setup owning the pipeline.
 * @param key Pipeline configuration key.
 * @return Cached or newly created pipeline, or NULL on failure.
 */
struct wsm_vk_pipeline *setup_get_or_create_pipeline(
	struct wsm_vk_render_format_setup *setup,
	const struct wsm_vk_pipeline_key *key);

/**
 * @brief Gets or creates a texture pipeline layout and sampler.
 * @param renderer Renderer owning the layout cache.
 * @param key Layout configuration key.
 * @return Cached or newly created layout, or NULL on failure.
 */
struct wsm_vk_pipeline_layout *get_or_create_pipeline_layout(
	struct wsm_vk_renderer *renderer,
	const struct wsm_vk_pipeline_layout_key *key);

/**
 * @brief Gets or creates a texture view and descriptor set.
 * @param texture Texture receiving the cached view.
 * @param layout Pipeline layout used to sample the texture.
 * @param srgb Whether to use an sRGB image view.
 * @return Texture-owned view, or NULL on failure.
 */
struct wsm_vk_texture_view *vulkan_texture_get_or_create_view(
	struct wsm_vk_texture *texture,
	const struct wsm_vk_pipeline_layout *layout, bool srgb);

/**
 * @brief Creates a renderer using an initialized Vulkan device.
 * @details Takes ownership of the device and instance, including cleanup on failure.
 * @param dev Device transferred to the renderer, together with its instance.
 * @return Renderer, or NULL on failure.
 */
struct wlr_renderer *vulkan_renderer_create_for_device(struct wsm_vk_device *dev);

/**
 * @brief Gets a recording command buffer for staging transfers.
 * @details Staging commands are submitted before rendering commands for the next frame.
 * @param renderer Renderer whose staging commands are recorded.
 * @return Staging command buffer, or VK_NULL_HANDLE on failure.
 */
VkCommandBuffer vulkan_record_stage_cb(struct wsm_vk_renderer *renderer);

/**
 * @brief Submits staging commands and waits for their completion.
 * @param renderer Renderer with pending staging commands.
 * @param wait_sync_file_fd Optional synchronization file to wait on, or -1; consumed on successful
 *        import.
 * @return true on completion, false on failure or when no staging command buffer exists.
 */
bool vulkan_submit_stage_wait(struct wsm_vk_renderer *renderer, int wait_sync_file_fd);

/**
 * @brief Suballocates CPU-mapped storage for a staging transfer.
 * @details Call vulkan_stage_mark_submit() after allocation to associate storage reuse with GPU
 *          completion.
 * @param renderer Renderer owning staging buffers.
 * @param size Requested span size in bytes.
 * @param alignment Required byte alignment for the returned offset.
 * @return Allocated span, or a span with a NULL buffer on failure.
 */
struct wsm_vk_buffer_span vulkan_get_stage_span(
	struct wsm_vk_renderer *renderer, VkDeviceSize size,
	VkDeviceSize alignment);

/**
 * @brief Marks new staging allocations with their release timeline point.
 * @param renderer Renderer owning staging allocations.
 * @param timeline_point Timeline point after which allocations may be reclaimed.
 */
void vulkan_stage_mark_submit(struct wsm_vk_renderer *renderer,
	uint64_t timeline_point);

/**
 * @brief Allocates a descriptor set for texture sampling.
 * @param renderer Renderer owning descriptor pools.
 * @param ds_layout Descriptor set layout.
 * @param ds Receives the allocated descriptor set.
 * @return Pool owning the set, or NULL on failure.
 */
struct wsm_vk_descriptor_pool *vulkan_alloc_texture_ds(
	struct wsm_vk_renderer *renderer, VkDescriptorSetLayout ds_layout,
	VkDescriptorSet *ds);

/**
 * @brief Allocates a descriptor set for an intermediate blending image.
 * @param renderer Renderer owning output descriptor pools.
 * @param ds Receives the allocated descriptor set.
 * @return Pool owning the set, or NULL on failure.
 */
struct wsm_vk_descriptor_pool *vulkan_alloc_blend_ds(
	struct wsm_vk_renderer *renderer, VkDescriptorSet *ds);

/**
 * @brief Frees a descriptor set and updates its pool capacity.
 * @param renderer Renderer owning the descriptor pool.
 * @param pool Pool from which the set was allocated.
 * @param ds Descriptor set to release.
 */
void vulkan_free_ds(struct wsm_vk_renderer *renderer,
	struct wsm_vk_descriptor_pool *pool, VkDescriptorSet ds);

/**
 * @brief Acquires a reusable command buffer from the renderer pool.
 * @param renderer Renderer owning the command pool.
 * @return Command buffer, or NULL on failure.
 */
struct wsm_vk_command_buffer *vulkan_acquire_command_buffer(
	struct wsm_vk_renderer *renderer);

/**
 * @brief Ends recording and assigns a completion timeline point.
 * @details The caller must submit the command buffer and signal the assigned point.
 * @param cb Recording command buffer to end.
 * @param renderer Renderer providing the timeline sequence.
 * @return Assigned timeline point, or zero on failure.
 */
uint64_t vulkan_end_command_buffer(struct wsm_vk_command_buffer *cb,
	struct wsm_vk_renderer *renderer);

/**
 * @brief Resets a command buffer and clears its recording state.
 * @param cb Command buffer to reset; may be NULL.
 */
void vulkan_reset_command_buffer(struct wsm_vk_command_buffer *cb);

/**
 * @brief Waits for a submitted command buffer to complete.
 * @param cb Submitted command buffer whose recording has ended.
 * @param renderer Renderer owning the completion timeline.
 * @return true on completion, false on failure.
 */
bool vulkan_wait_command_buffer(struct wsm_vk_command_buffer *cb,
	struct wsm_vk_renderer *renderer);

/**
 * @brief Imports a synchronization file into a command buffer semaphore.
 * @param renderer Renderer owning the semaphore.
 * @param render_cb Command buffer retaining the wait semaphore.
 * @param sem_index Index of the reusable wait semaphore slot.
 * @param sync_file_fd Synchronization file descriptor; consumed on successful import.
 * @return Wait semaphore, or VK_NULL_HANDLE on failure.
 */
VkSemaphore vulkan_command_buffer_wait_sync_file(struct wsm_vk_renderer *renderer,
	struct wsm_vk_command_buffer *render_cb, size_t sem_index, int sync_file_fd);

/**
 * @brief Publishes render completion to explicit or implicit synchronization.
 * @details Blocks for command completion when implicit sync interoperability and an explicit
 *          signal timeline are both unavailable.
 * @param renderer Renderer submitting the pass.
 * @param pass Submitted render pass to release.
 * @return true on success, false on failure.
 */
bool vulkan_sync_render_pass_release(struct wsm_vk_renderer *renderer,
	struct wsm_vk_render_pass *pass);

/**
 * @brief Descriptor pool and remaining allocation capacity.
 */
struct wsm_vk_descriptor_pool {
	VkDescriptorPool pool; /**< Vulkan descriptor pool handle. */
	uint32_t free; /**< Number of descriptor sets still available for allocation. */
	struct wl_list link; /**< Link in a renderer descriptor pool list. */
};

/**
 * @brief Staging ring position associated with a submission timeline point.
 */
struct wsm_vk_stage_watermark {
	VkDeviceSize head; /**< Ring head position reclaimed when the timeline point completes. */
	uint64_t timeline_point; /**< Renderer timeline point associated with completion. */
};

/**
 * @brief CPU-mapped ring buffer used for staging transfers.
 */
struct wsm_vk_stage_buffer {
	struct wl_list link; /**< Link in wsm_vk_renderer.stage.buffers. */
	VkBuffer buffer; /**< Vulkan transfer buffer handle. */
	VkDeviceMemory memory; /**< Memory backing the staging buffer. */
	VkDeviceSize buf_size; /**< Staging buffer capacity in bytes. */
	void *cpu_mapping; /**< Persistent CPU mapping of staging memory. */

	VkDeviceSize head; /**< Ring position after the latest allocation. */
	VkDeviceSize tail; /**< Oldest ring position not yet reclaimed. */

	struct wl_array watermarks; /**< Array of wsm_vk_stage_watermark entries awaiting completion. */
	int empty_gc_cnt; /**< Consecutive garbage collection passes with no live allocations. */
};

/**
 * @brief Suballocated byte range within a staging ring buffer.
 */
struct wsm_vk_buffer_span {
	struct wsm_vk_stage_buffer *buffer; /**< Staging buffer containing the range; NULL on allocation failure. */
	VkDeviceSize offset; /**< Byte offset of the suballocated span. */
	VkDeviceSize size; /**< Size of the suballocated span in bytes. */
};

/**
 * @brief Allocates an aligned byte range in a staging ring buffer.
 * @param buf Staging ring buffer to allocate from.
 * @param size Requested byte count.
 * @param alignment Required byte alignment.
 * @return Byte offset, or (VkDeviceSize)-1 when no range fits.
 */
VkDeviceSize wsm_vk_stage_buffer_alloc(struct wsm_vk_stage_buffer *buf,
	VkDeviceSize size, VkDeviceSize alignment);

/**
 * @brief Reclaims staging ranges whose submission has completed.
 * @param buf Staging ring buffer to update.
 * @param current_point Latest completed timeline point.
 */
void wsm_vk_stage_buffer_reclaim(struct wsm_vk_stage_buffer *buf,
	uint64_t current_point);

/**
 * @brief Renderer resources for a matrix or three-dimensional lookup transform.
 */
struct wsm_vk_color_transform {
	struct wlr_addon addon; /**< Addon attaching these resources to their source object. */
	struct wl_list link; /**< Link in wsm_vk_renderer.color_transforms. */

	struct {
		size_t dim; /**< Number of lookup table samples per color axis. */
		VkImage image; /**< Vulkan image handle. */
		VkImageView image_view; /**< Vulkan image view handle. */
		VkDeviceMemory memory; /**< Memory backing the color lookup table image. */
		VkDescriptorSet ds; /**< Vulkan descriptor set handle. */
		struct wsm_vk_descriptor_pool *ds_pool; /**< Pool owning the descriptor set. */
	} lut_3d; /**< Full transform lookup table; overrides matrix parameters when populated. */

	float color_matrix[9]; /**< Matrix for color conversion when no full lookup table is used. */
	enum wlr_color_transfer_function inverse_eotf; /**< Output transfer function applied after the color matrix. */
};

/**
 * @brief Destroys renderer resources attached to a color transform.
 * @param addon Addon identifying the prepared Vulkan color transform.
 */
void wsm_vk_color_transform_destroy(struct wlr_addon *addon);

/**
 * @brief Checks whether a named Vulkan extension is available.
 * @param avail Available extension properties.
 * @param avail_len Number of available extensions.
 * @param name Extension name to find.
 * @return true when the extension is present, false otherwise.
 */
bool wsm_vk_check_extension(const VkExtensionProperties *avail,
	uint32_t avail_len, const char *name);

/**
 * @brief Gets a readable description of a Vulkan result code.
 * @param err Vulkan result code.
 * @return Static result description.
 */
const char *wsm_vk_strerror(VkResult err);

/**
 * @brief Records an image layout transition and memory dependency.
 * @param cb Command buffer recording the barrier.
 * @param img Image to transition.
 * @param ol Previous image layout.
 * @param srcs Source pipeline stage mask.
 * @param srca Source access mask.
 * @param nl New image layout.
 * @param dsts Destination pipeline stage mask.
 * @param dsta Destination access mask.
 */
void wsm_vk_change_layout(VkCommandBuffer cb, VkImage img,
	VkImageLayout ol, VkPipelineStageFlags srcs, VkAccessFlags srca,
	VkImageLayout nl, VkPipelineStageFlags dsts, VkAccessFlags dsta);

#if __STDC_VERSION__ >= 202311L

/**
 * @brief Logs a Vulkan failure with its readable result and numeric code.
 * @param fmt Printf-style message format.
 * @param res Vulkan result code.
 * @param ... Additional message format arguments.
 */
#define wsm_vk_error(fmt, res, ...) wsm_log(WSM_ERROR, fmt ": %s (%d)", \
	wsm_vk_strerror(res), res __VA_OPT__(,) __VA_ARGS__)

#else

/**
 * @brief Logs a Vulkan failure with its readable result and numeric code.
 * @param fmt Printf-style message format.
 * @param res Vulkan result code.
 * @param ... Additional message format arguments.
 */
#define wsm_vk_error(fmt, res, ...) wsm_log(WSM_ERROR, fmt ": %s (%d)", \
	wsm_vk_strerror(res), res, ##__VA_ARGS__)

#endif

/**
 * @brief Gets or creates render passes for a target format.
 * @param renderer Renderer owning the setup cache.
 * @param format Output image format mapping.
 * @param has_blending_buffer Whether rendering uses an intermediate blending image.
 * @param srgb Whether the output attachment uses an sRGB view.
 * @return Cached or newly created format setup, or NULL on failure.
 */
struct wsm_vk_render_format_setup *find_or_create_render_setup(
	struct wsm_vk_renderer *renderer, const struct wsm_vk_format *format,
	bool has_blending_buffer, bool srgb);

/**
 * @brief Gets the queue family used by a Vulkan renderer.
 * @param renderer Renderer known to use the Vulkan implementation.
 * @return Rendering and transfer queue family index.
 */
uint32_t wsm_vk_renderer_get_queue_family(struct wlr_renderer *renderer);

/**
 * @brief Checks whether a renderer uses the Vulkan implementation.
 * @param wlr_renderer Renderer to inspect.
 * @return true for a Vulkan renderer, false otherwise.
 */
bool wlr_renderer_is_wsm_vk(const struct wlr_renderer *wlr_renderer);

/**
 * @brief Casts a generic renderer to a Vulkan renderer.
 * @details Asserts that the renderer has the expected implementation.
 * @param r Renderer known to use the Vulkan implementation.
 * @return Enclosing Vulkan renderer.
 */
struct wsm_vk_renderer *wsm_vk_renderer_from_renderer(struct wlr_renderer *r);

#endif // RENDER_VULKAN_H
