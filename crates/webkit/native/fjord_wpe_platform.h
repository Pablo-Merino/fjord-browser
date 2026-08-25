#ifndef FJORD_WPE_PLATFORM_H
#define FJORD_WPE_PLATFORM_H

#include <glib.h>
#include <wpe/wpe-platform.h>

struct wl_surface;
struct zwp_linux_dmabuf_v1;

typedef enum {
    FJORD_WPE_FRAME_ATTACHED,
    FJORD_WPE_FRAME_RENDERED,
    FJORD_WPE_FRAME_RELEASED,
} FjordWpeFrameEvent;

typedef void (*FjordWpeFrameEventCallback)(gpointer data, FjordWpeFrameEvent event);

WPEDisplay *fjord_wpe_display_new(
    struct wl_surface *surface,
    struct zwp_linux_dmabuf_v1 *dmabuf,
    const guint *buffer_scale,
    FjordWpeFrameEventCallback frame_event,
    gpointer frame_event_data
);

void fjord_wpe_display_shutdown(WPEDisplay *display);

#endif
