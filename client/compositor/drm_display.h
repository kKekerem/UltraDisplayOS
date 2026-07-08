#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
    uint32_t crtc_id;
    uint32_t connector_id;
    
    // Primary video plane
    uint32_t primary_plane_id;
    
    // Hardware cursor plane
    uint32_t cursor_plane_id;
    
    // Overlay plane (for TUI while streaming)
    uint32_t overlay_plane_id;

    drmModeModeInfo mode;
    bool vrr_capable;
    bool hdr_capable;
} DrmContext;

// Initialize DRM/KMS on the primary display. Handles Apple dual-GPU muxing if necessary.
bool drm_init(DrmContext* ctx);

// Commit a DMA-BUF to the primary scanout plane immediately.
// Uses atomic page flip. Blocks if previous flip is pending.
bool drm_scanout_dmabuf(DrmContext* ctx, int dma_buf_fd, uint32_t width, uint32_t height, uint32_t format);

// Sets FreeSync/VRR properties on the CRTC.
bool drm_set_vrr(DrmContext* ctx, bool enable);

// Moves the hardware cursor without affecting the main plane.
bool drm_move_cursor(DrmContext* ctx, int x, int y);

// Update cursor shape from ARGB buffer.
bool drm_update_cursor_shape(DrmContext* ctx, const uint32_t* argb_pixels, uint32_t width, uint32_t height);

void drm_cleanup(DrmContext* ctx);

#ifdef __cplusplus
}
#endif
