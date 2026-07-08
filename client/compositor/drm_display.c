#include "drm_display.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <drm_fourcc.h>

bool drm_init(DrmContext* ctx) {
    memset(ctx, 0, sizeof(DrmContext));
    
    // Simplistic node finder. In production, iterate /dev/dri/cardX and check capabilities
    ctx->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (ctx->fd < 0) {
        perror("Failed to open DRM node");
        return false;
    }

    // Enable atomic capability
    if (drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_ATOMIC, 1) < 0) {
        fprintf(stderr, "Atomic modesetting not supported\n");
        close(ctx->fd);
        return false;
    }

    // Get resources
    drmModeRes* res = drmModeGetResources(ctx->fd);
    if (!res) {
        close(ctx->fd);
        return false;
    }

    // Find first connected connector
    drmModeConnector* conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(ctx->fd, res->connectors[i]);
        if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            ctx->connector_id = conn->connector_id;
            ctx->mode = conn->modes[0]; // Select highest resolution mode by default
            break;
        }
        if (conn) drmModeFreeConnector(conn);
        conn = NULL;
    }

    if (!conn) {
        fprintf(stderr, "No connected display found\n");
        drmModeFreeResources(res);
        return false;
    }

    // Find suitable CRTC
    for (int i = 0; i < res->count_encoders; i++) {
        drmModeEncoder* enc = drmModeGetEncoder(ctx->fd, res->encoders[i]);
        if (enc && enc->encoder_id == conn->encoder_id) {
            ctx->crtc_id = enc->crtc_id;
            drmModeFreeEncoder(enc);
            break;
        }
        if (enc) drmModeFreeEncoder(enc);
    }

    // Fallback: use first CRTC if not explicitly bound
    if (ctx->crtc_id == 0 && res->count_crtcs > 0) {
        ctx->crtc_id = res->crtcs[0];
    }

    // Plane setup (simplified: just grab first primary and first cursor)
    drmModePlaneRes* plane_res = drmModeGetPlaneResources(ctx->fd);
    if (plane_res) {
        for (uint32_t i = 0; i < plane_res->count_planes; i++) {
            drmModePlane* plane = drmModeGetPlane(ctx->fd, plane_res->planes[i]);
            if (!plane) continue;

            // Need to check properties to find plane type (Primary, Cursor, Overlay)
            // Simplified for prototype:
            if (ctx->primary_plane_id == 0) ctx->primary_plane_id = plane->plane_id;
            
            drmModeFreePlane(plane);
        }
        drmModeFreePlaneResources(plane_res);
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    printf("DRM Init: CRTC=%u, Connector=%u, PrimaryPlane=%u\n", 
           ctx->crtc_id, ctx->connector_id, ctx->primary_plane_id);
    return true;
}

bool drm_scanout_dmabuf(DrmContext* ctx, int dma_buf_fd, uint32_t width, uint32_t height, uint32_t format) {
    if (!ctx || ctx->fd < 0) return false;

    uint32_t fb_id = 0;
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};

    // 1. Import DMA-BUF as DRM prime handle
    if (drmPrimeFDToHandle(ctx->fd, dma_buf_fd, &handles[0]) < 0) {
        perror("Failed to import DMA-BUF");
        return false;
    }

    // NV12 typical pitch calculation (assuming tightly packed)
    pitches[0] = width; // Y plane
    offsets[0] = 0;
    
    if (format == DRM_FORMAT_NV12) {
        handles[1] = handles[0];
        pitches[1] = width; // UV plane
        offsets[1] = width * height;
    }

    // 2. Add Framebuffer
    if (drmModeAddFB2(ctx->fd, width, height, format, handles, pitches, offsets, &fb_id, 0) < 0) {
        perror("Failed to add DRM FB");
        return false;
    }

    // 3. Atomic commit (simplified, requires property IDs in reality)
    drmModeAtomicReq* req = drmModeAtomicAlloc();
    
    // In a real implementation, we'd lookup the property IDs for FB_ID, CRTC_ID, SRC_X, CRTC_X, etc.
    // For brevity in this architectural prototype, we omit the full property mapping loop.
    
    // ... drmModeAtomicAddProperty calls ...

    // int ret = drmModeAtomicCommit(ctx->fd, req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, ctx);
    
    drmModeAtomicFree(req);

    // Normally we'd queue fb_id for deletion after the next page flip completes.
    
    return true;
}

bool drm_set_vrr(DrmContext* ctx, bool enable) {
    // Requires finding the "VRR_ENABLED" property on the CRTC and atomic-committing it
    return true; 
}

bool drm_move_cursor(DrmContext* ctx, int x, int y) {
    if (ctx->cursor_plane_id == 0) return false;
    // Atomic commit to update CRTC_X and CRTC_Y properties of the cursor plane
    return true;
}

void drm_cleanup(DrmContext* ctx) {
    if (ctx && ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}
