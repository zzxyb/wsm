/* SPDX-License-Identifier: MIT */

/**
 * @file        dmabuf.h
 * @brief       DMA-BUF synchronization file interoperability.
 * @details     Declares helpers for importing and exporting fences between explicit and implicit
 *              synchronization.
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

#ifndef RENDER_DMABUF_H
#define RENDER_DMABUF_H

#include <stdbool.h>
#include <stdint.h>

// Copied from <linux/dma-buf.h> to avoid #ifdef soup
/** @brief Access flag for reading a DMA-BUF. */
#define DMA_BUF_SYNC_READ      (1 << 0)
/** @brief Access flag for writing a DMA-BUF. */
#define DMA_BUF_SYNC_WRITE     (2 << 0)
/** @brief Combined DMA-BUF read and write access flags. */
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)

/**
 * @brief Checks platform support for DMA-BUF synchronization file ioctls.
 * @return true when synchronization file import and export are available.
 */
bool dmabuf_check_sync_file_import_export(void);

/**
 * @brief Imports an explicit fence into DMA-BUF implicit synchronization.
 * @param dmabuf_fd DMA-BUF file descriptor.
 * @param flags DMA_BUF_SYNC_READ and/or DMA_BUF_SYNC_WRITE access flags.
 * @param sync_file_fd Synchronization file descriptor; ownership remains with the caller.
 * @return true on success, false on failure.
 */
bool dmabuf_import_sync_file(int dmabuf_fd, uint32_t flags, int sync_file_fd);

/**
 * @brief Exports a DMA-BUF implicit fence as a synchronization file.
 * @param dmabuf_fd DMA-BUF file descriptor.
 * @param flags DMA_BUF_SYNC_READ and/or DMA_BUF_SYNC_WRITE access flags.
 * @return Owned synchronization file descriptor, or -1 on failure.
 */
int dmabuf_export_sync_file(int dmabuf_fd, uint32_t flags);

#endif
