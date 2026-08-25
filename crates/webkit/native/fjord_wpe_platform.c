#include "fjord_wpe_platform.h"

#include <wpe/headless/wpe-headless.h>
#include <wayland-client-protocol.h>
#include "linux-dmabuf-client-protocol.h"

typedef struct {
    struct wl_surface *surface;
    struct zwp_linux_dmabuf_v1 *dmabuf;
    const guint *buffer_scale;
    FjordWpeFrameEventCallback frame_event;
    gpointer frame_event_data;
} FjordWpePlatformConfig;

typedef struct {
    GWeakRef view;
    WPEBuffer *wpe_buffer;
    struct wl_buffer *wl_buffer;
    const gboolean *shutting_down;
    gboolean in_flight;
    gboolean releasing;
} FjordWaylandBuffer;

typedef struct _FjordWpeView {
    WPEView parent_instance;
    FjordWpePlatformConfig config;
    struct wl_callback *frame_callback;
    WPEBuffer *rendered_buffer;
    WPEBuffer *pending_buffer;
} FjordWpeView;

typedef struct _FjordWpeViewClass {
    WPEViewClass parent_class;
} FjordWpeViewClass;

typedef struct _FjordWpeDisplay {
    WPEDisplay parent_instance;
    WPEDisplay *device_display;
    FjordWpePlatformConfig config;
    WPEView *active_view;
    gboolean shutting_down;
} FjordWpeDisplay;

typedef struct _FjordWpeDisplayClass {
    WPEDisplayClass parent_class;
} FjordWpeDisplayClass;

G_DEFINE_TYPE(FjordWpeView, fjord_wpe_view, WPE_TYPE_VIEW)
G_DEFINE_TYPE(FjordWpeDisplay, fjord_wpe_display, WPE_TYPE_DISPLAY)

static void frame_event(FjordWpeView *view, FjordWpeFrameEvent event) {
    if (view->config.frame_event)
        view->config.frame_event(view->config.frame_event_data, event);
}

static void wayland_buffer_free(FjordWaylandBuffer *buffer) {
    if (buffer->wl_buffer)
        wl_buffer_destroy(buffer->wl_buffer);
    g_weak_ref_clear(&buffer->view);
    g_free(buffer);
}

static void wayland_buffer_destroy(gpointer data) {
    FjordWaylandBuffer *buffer = data;

    if (!buffer)
        return;
    buffer->wpe_buffer = NULL;
    if ((!buffer->in_flight || *buffer->shutting_down) && !buffer->releasing)
        wayland_buffer_free(buffer);
}

static void wayland_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    FjordWaylandBuffer *buffer = data;
    FjordWpeView *view;

    if (!buffer || buffer->wl_buffer != wl_buffer || !buffer->in_flight)
        return;
    buffer->in_flight = FALSE;
    view = g_weak_ref_get(&buffer->view);
    if (!view) {
        if (!buffer->wpe_buffer)
            wayland_buffer_free(buffer);
        return;
    }
    buffer->releasing = TRUE;
    if (buffer->wpe_buffer)
        wpe_view_buffer_released(WPE_VIEW(view), buffer->wpe_buffer);
    buffer->releasing = FALSE;
    frame_event(view, FJORD_WPE_FRAME_RELEASED);
    g_object_unref(view);
    if (!buffer->wpe_buffer)
        wayland_buffer_free(buffer);
}

static const struct wl_buffer_listener wayland_buffer_listener = {
    .release = wayland_buffer_release,
};

static gboolean present_buffer(FjordWpeView *view, WPEBuffer *wpe_buffer, GError **error);

static gboolean view_is_active(FjordWpeView *view) {
    FjordWpeDisplay *display = (FjordWpeDisplay *)wpe_view_get_display(WPE_VIEW(view));

    return display->active_view == WPE_VIEW(view);
}

static struct wl_buffer *create_wayland_buffer(
    FjordWpeView *view,
    WPEBuffer *wpe_buffer,
    GError **error
) {
    FjordWaylandBuffer *buffer = wpe_buffer_get_user_data(wpe_buffer);
    WPEBufferDMABuf *dma_buf;
    struct zwp_linux_buffer_params_v1 *params;
    guint planes;
    guint64 modifier;

    if (buffer)
        return buffer->wl_buffer;
    if (!WPE_IS_BUFFER_DMA_BUF(wpe_buffer)) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Fjord WPE view received a non-dma-buf buffer"
        );
        return NULL;
    }

    dma_buf = WPE_BUFFER_DMA_BUF(wpe_buffer);
    planes = wpe_buffer_dma_buf_get_n_planes(dma_buf);
    if (!planes || planes > 4) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Fjord WPE view received an unsupported dma-buf plane count"
        );
        return NULL;
    }

    params = zwp_linux_dmabuf_v1_create_params(view->config.dmabuf);
    if (!params) {
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Fjord WPE view failed to create dma-buf parameters"
        );
        return NULL;
    }
    modifier = wpe_buffer_dma_buf_get_modifier(dma_buf);
    for (guint plane = 0; plane < planes; plane++) {
        zwp_linux_buffer_params_v1_add(
            params,
            wpe_buffer_dma_buf_get_fd(dma_buf, plane),
            plane,
            wpe_buffer_dma_buf_get_offset(dma_buf, plane),
            wpe_buffer_dma_buf_get_stride(dma_buf, plane),
            modifier >> 32,
            modifier & G_MAXUINT32
        );
    }

    buffer = g_new0(FjordWaylandBuffer, 1);
    buffer->wpe_buffer = wpe_buffer;
    buffer->shutting_down = &((FjordWpeDisplay *)wpe_view_get_display(WPE_VIEW(view)))->shutting_down;
    g_weak_ref_init(&buffer->view, view);
    buffer->wl_buffer = zwp_linux_buffer_params_v1_create_immed(
        params,
        wpe_buffer_get_width(wpe_buffer),
        wpe_buffer_get_height(wpe_buffer),
        wpe_buffer_dma_buf_get_format(dma_buf),
        0
    );
    zwp_linux_buffer_params_v1_destroy(params);
    if (!buffer->wl_buffer) {
        wayland_buffer_free(buffer);
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Fjord WPE view failed to create a Wayland dma-buf"
        );
        return NULL;
    }

    wl_buffer_add_listener(buffer->wl_buffer, &wayland_buffer_listener, buffer);
    wpe_buffer_set_user_data(wpe_buffer, buffer, wayland_buffer_destroy);
    return buffer->wl_buffer;
}

static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    FjordWpeView *view = data;
    WPEBuffer *buffer;
    WPEBuffer *pending_buffer;
    GError *error = NULL;

    (void)time;
    if (view->frame_callback != callback)
        return;
    wl_callback_destroy(callback);
    view->frame_callback = NULL;
    buffer = view->rendered_buffer;
    view->rendered_buffer = NULL;
    pending_buffer = view->pending_buffer;
    view->pending_buffer = NULL;
    if (pending_buffer) {
        if (!view_is_active(view)) {
            wpe_view_buffer_rendered(WPE_VIEW(view), pending_buffer);
            wpe_view_buffer_released(WPE_VIEW(view), pending_buffer);
        } else if (!present_buffer(view, pending_buffer, &error)) {
            g_printerr("Fjord WPE pending frame failed: %s\n", error->message);
            g_clear_error(&error);
            wpe_view_buffer_rendered(WPE_VIEW(view), pending_buffer);
            wpe_view_buffer_released(WPE_VIEW(view), pending_buffer);
        }
        g_object_unref(pending_buffer);
    }
    wpe_view_buffer_rendered(WPE_VIEW(view), buffer);
    frame_event(view, FJORD_WPE_FRAME_RENDERED);
    g_object_unref(buffer);
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

static gboolean present_buffer(FjordWpeView *view, WPEBuffer *wpe_buffer, GError **error) {
    FjordWaylandBuffer *buffer;
    struct wl_buffer *wl_buffer;
    guint buffer_scale;

    wl_buffer = create_wayland_buffer(view, wpe_buffer, error);
    if (!wl_buffer)
        return FALSE;
    buffer = wpe_buffer_get_user_data(wpe_buffer);
    buffer->in_flight = TRUE;
    view->rendered_buffer = g_object_ref(wpe_buffer);
    view->frame_callback = wl_surface_frame(view->config.surface);
    if (!view->frame_callback) {
        buffer->in_flight = FALSE;
        g_clear_object(&view->rendered_buffer);
        g_set_error_literal(
            error,
            WPE_VIEW_ERROR,
            WPE_VIEW_ERROR_RENDER_FAILED,
            "Fjord WPE view failed to create a Wayland frame callback"
        );
        return FALSE;
    }
    wl_callback_add_listener(view->frame_callback, &frame_listener, view);
    buffer_scale = MAX(*view->config.buffer_scale, 1);
    if (wpe_buffer_get_width(wpe_buffer) == wpe_view_get_width(WPE_VIEW(view)) * (int)buffer_scale &&
        wpe_buffer_get_height(wpe_buffer) == wpe_view_get_height(WPE_VIEW(view)) * (int)buffer_scale)
        wl_surface_set_buffer_scale(view->config.surface, buffer_scale);
    wl_surface_attach(view->config.surface, wl_buffer, 0, 0);
    wl_surface_damage_buffer(
        view->config.surface,
        0,
        0,
        wpe_buffer_get_width(wpe_buffer),
        wpe_buffer_get_height(wpe_buffer)
    );
    wl_surface_commit(view->config.surface);
    frame_event(view, FJORD_WPE_FRAME_ATTACHED);
    return TRUE;
}

static gboolean fjord_wpe_view_render_buffer(
    WPEView *wpe_view,
    WPEBuffer *wpe_buffer,
    const WPERectangle *damage_rects,
    guint damage_rect_count,
    GError **error
) {
    FjordWpeView *view = (FjordWpeView *)wpe_view;

    (void)damage_rects;
    (void)damage_rect_count;
    if (!view_is_active(view)) {
        wpe_view_buffer_rendered(wpe_view, wpe_buffer);
        wpe_view_buffer_released(wpe_view, wpe_buffer);
        return TRUE;
    }
    if (!view->frame_callback)
        return present_buffer(view, wpe_buffer, error);

    if (view->pending_buffer) {
        wpe_view_buffer_rendered(wpe_view, view->pending_buffer);
        wpe_view_buffer_released(wpe_view, view->pending_buffer);
        g_clear_object(&view->pending_buffer);
    }
    view->pending_buffer = g_object_ref(wpe_buffer);
    return TRUE;
}

static void toplevel_changed(WPEView *view, GParamSpec *spec, gpointer data) {
    WPEToplevel *toplevel;
    int width;
    int height;

    (void)spec;
    (void)data;
    toplevel = wpe_view_get_toplevel(view);
    if (!toplevel) {
        wpe_view_unmap(view);
        return;
    }
    wpe_toplevel_get_size(toplevel, &width, &height);
    if (width > 0 && height > 0)
        wpe_view_resized(view, width, height);
    wpe_view_map(view);
}

static void fjord_wpe_view_constructed(GObject *object) {
    G_OBJECT_CLASS(fjord_wpe_view_parent_class)->constructed(object);
    g_signal_connect(object, "notify::toplevel", G_CALLBACK(toplevel_changed), NULL);
}

static void fjord_wpe_view_dispose(GObject *object) {
    FjordWpeView *view = (FjordWpeView *)object;
    FjordWpeDisplay *display = (FjordWpeDisplay *)wpe_view_get_display(WPE_VIEW(view));

    if (display->active_view == WPE_VIEW(view))
        display->active_view = NULL;
    if (view->frame_callback) {
        wl_callback_destroy(view->frame_callback);
        view->frame_callback = NULL;
    }
    g_clear_object(&view->rendered_buffer);
    g_clear_object(&view->pending_buffer);
    G_OBJECT_CLASS(fjord_wpe_view_parent_class)->dispose(object);
}

static void fjord_wpe_view_class_init(FjordWpeViewClass *view_class) {
    GObjectClass *object_class = G_OBJECT_CLASS(view_class);
    WPEViewClass *wpe_view_class = WPE_VIEW_CLASS(view_class);

    object_class->constructed = fjord_wpe_view_constructed;
    object_class->dispose = fjord_wpe_view_dispose;
    wpe_view_class->render_buffer = fjord_wpe_view_render_buffer;
}

static void fjord_wpe_view_init(FjordWpeView *view) {
    (void)view;
}

static gboolean fjord_wpe_display_connect(WPEDisplay *display, GError **error) {
    return wpe_display_connect(((FjordWpeDisplay *)display)->device_display, error);
}

static WPEView *fjord_wpe_display_create_view(WPEDisplay *display) {
    FjordWpeDisplay *fjord_display = (FjordWpeDisplay *)display;
    FjordWpeView *view = g_object_new(fjord_wpe_view_get_type(), "display", display, NULL);

    view->config = fjord_display->config;
    if (!fjord_display->active_view)
        fjord_display->active_view = WPE_VIEW(view);
    return WPE_VIEW(view);
}

static WPEToplevel *fjord_wpe_display_create_toplevel(WPEDisplay *display, guint max_views) {
    (void)max_views;
    return WPE_TOPLEVEL(g_object_new(WPE_TYPE_TOPLEVEL_HEADLESS, "display", display, NULL));
}

static gpointer fjord_wpe_display_get_egl_display(WPEDisplay *display, GError **error) {
    return wpe_display_get_egl_display(((FjordWpeDisplay *)display)->device_display, error);
}

static WPEDRMDevice *fjord_wpe_display_get_drm_device(WPEDisplay *display) {
    return wpe_display_get_drm_device(((FjordWpeDisplay *)display)->device_display);
}

static void fjord_wpe_display_dispose(GObject *object) {
    FjordWpeDisplay *display = (FjordWpeDisplay *)object;

    g_clear_object(&display->device_display);
    G_OBJECT_CLASS(fjord_wpe_display_parent_class)->dispose(object);
}

static void fjord_wpe_display_class_init(FjordWpeDisplayClass *display_class) {
    GObjectClass *object_class = G_OBJECT_CLASS(display_class);
    WPEDisplayClass *wpe_display_class = WPE_DISPLAY_CLASS(display_class);

    object_class->dispose = fjord_wpe_display_dispose;
    wpe_display_class->connect = fjord_wpe_display_connect;
    wpe_display_class->create_view = fjord_wpe_display_create_view;
    wpe_display_class->create_toplevel = fjord_wpe_display_create_toplevel;
    wpe_display_class->get_egl_display = fjord_wpe_display_get_egl_display;
    wpe_display_class->get_drm_device = fjord_wpe_display_get_drm_device;
}

static void fjord_wpe_display_init(FjordWpeDisplay *display) {
    display->device_display = wpe_display_headless_new();
    wpe_display_set_available_input_devices(
        WPE_DISPLAY(display),
        WPE_AVAILABLE_INPUT_DEVICE_MOUSE | WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD
    );
}

WPEDisplay *fjord_wpe_display_new(
    struct wl_surface *surface,
    struct zwp_linux_dmabuf_v1 *dmabuf,
    const guint *buffer_scale,
    FjordWpeFrameEventCallback frame_event_callback,
    gpointer frame_event_data
) {
    FjordWpeDisplay *display;

    g_return_val_if_fail(surface, NULL);
    g_return_val_if_fail(dmabuf, NULL);
    g_return_val_if_fail(buffer_scale, NULL);
    display = g_object_new(fjord_wpe_display_get_type(), NULL);
    display->config.surface = surface;
    display->config.dmabuf = dmabuf;
    display->config.buffer_scale = buffer_scale;
    display->config.frame_event = frame_event_callback;
    display->config.frame_event_data = frame_event_data;
    return WPE_DISPLAY(display);
}

void fjord_wpe_display_shutdown(WPEDisplay *display) {
    g_return_if_fail(display && G_TYPE_CHECK_INSTANCE_TYPE(display, fjord_wpe_display_get_type()));
    ((FjordWpeDisplay *)display)->shutting_down = TRUE;
}

void fjord_wpe_display_set_active_view(WPEDisplay *display, WPEView *view) {
    g_return_if_fail(display && G_TYPE_CHECK_INSTANCE_TYPE(display, fjord_wpe_display_get_type()));
    g_return_if_fail(WPE_IS_VIEW(view));
    ((FjordWpeDisplay *)display)->active_view = view;
}
