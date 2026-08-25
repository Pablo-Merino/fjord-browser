#include "wpe_smoke.h"

#include <dirent.h>
#include <errno.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpe/headless/wpe-headless.h>
#include <wpe/webkit.h>
#include <wayland-client-protocol.h>
#include "linux-dmabuf-client-protocol.h"

typedef struct {
    GMainContext *context;
    GMainLoop *loop;
    WebKitWebView *web_view;
    FjordWpeSmokeReport *report;
    const char *expected_title;
    const char *expected_uri;
    char *error_message;
    gboolean termination_requested;
    gboolean terminate_web_process;
} SmokeState;

static void set_error(SmokeState *state, const char *message) {
    if (!state->error_message)
        state->error_message = g_strdup(message);
}

void fjord_wpe_smoke_close_fds(FjordWpeSmokeReport *report) {
    for (guint plane = 0; plane < G_N_ELEMENTS(report->dma_buf_fds); plane++) {
        if (report->dma_buf_fds[plane] >= 0)
            close(report->dma_buf_fds[plane]);
        report->dma_buf_fds[plane] = -1;
    }
}

static void reset_report(FjordWpeSmokeReport *report) {
    fjord_wpe_smoke_close_fds(report);
    memset(report, 0, sizeof(*report));
    for (guint plane = 0; plane < G_N_ELEMENTS(report->dma_buf_fds); plane++)
        report->dma_buf_fds[plane] = -1;
}

typedef struct {
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct zwp_linux_dmabuf_v1 *dmabuf;
    uint32_t dmabuf_version;
    gboolean supports_linear_xrgb;
} SubsurfaceProbe;

typedef struct {
    struct wl_buffer *buffer;
    gboolean failed;
} BufferCreation;

typedef struct LiveSmoke LiveSmoke;

typedef struct {
    struct wl_display *display;
    struct wl_event_queue *queue;
    struct zwp_linux_dmabuf_v1 *dmabuf;
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    WPEView *view;
    WPEBuffer *wpe_buffer;
    WPEToplevel *toplevel;
    GMainLoop *loop;
    char **error_message;
    LiveSmoke *smoke;
    guint index;
    gboolean in_flight;
} LiveBuffer;

struct LiveSmoke {
    GMainLoop *loop;
    char **error_message;
    guint active_view;
    guint released_frames;
    guint target_frames;
    guint resize_count;
    guint target_resizes;
};

static void live_buffer_fail(LiveBuffer *live, const char *message);

static void live_buffer_release(void *data, struct wl_buffer *buffer) {
    LiveBuffer *live = data;
    LiveSmoke *smoke;

    (void)buffer;
    if (!live)
        return;
    if (!live->in_flight)
        return;
    live->in_flight = FALSE;
    if (live->buffer == buffer) {
        wl_buffer_destroy(live->buffer);
        live->buffer = NULL;
    }
    smoke = live->smoke;
    smoke->released_frames++;
    smoke->active_view = (live->index + 1) % 2;
    wpe_view_buffer_released(live->view, live->wpe_buffer);
    if (smoke->resize_count < smoke->target_resizes) {
        static const gint sizes[][2] = {
            { 640, 480 },
            { 1024, 768 },
            { 800, 600 },
            { 720, 540 },
        };
        const gint *size = sizes[smoke->resize_count % G_N_ELEMENTS(sizes)];

        smoke->resize_count++;
        if (!wpe_toplevel_resize(live->toplevel, size[0], size[1])) {
            live_buffer_fail(live, "failed to resize the live WPE toplevel");
            return;
        }
    }
    if (smoke->released_frames >= smoke->target_frames)
        g_main_loop_quit(live->loop);
}

static const struct wl_buffer_listener live_buffer_listener = {
    .release = live_buffer_release,
};

static void buffer_created(
    void *data,
    struct zwp_linux_buffer_params_v1 *params,
    struct wl_buffer *buffer
) {
    (void)params;
    ((BufferCreation *)data)->buffer = buffer;
}

static void buffer_creation_failed(void *data, struct zwp_linux_buffer_params_v1 *params) {
    (void)params;
    ((BufferCreation *)data)->failed = TRUE;
}

static const struct zwp_linux_buffer_params_v1_listener buffer_creation_listener = {
    .created = buffer_created,
    .failed = buffer_creation_failed,
};

static void dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *dmabuf, uint32_t format) {
    (void)data;
    (void)dmabuf;
    (void)format;
}

static void dmabuf_modifier(
    void *data,
    struct zwp_linux_dmabuf_v1 *dmabuf,
    uint32_t format,
    uint32_t modifier_high,
    uint32_t modifier_low
) {
    (void)data;
    (void)dmabuf;
    (void)format;
    (void)modifier_high;
    (void)modifier_low;
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
    .format = dmabuf_format,
    .modifier = dmabuf_modifier,
};

static void subsurface_probe_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {
    SubsurfaceProbe *probe = data;

    if (!g_strcmp0(interface, "wl_compositor"))
        probe->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, MIN(version, 4));
    else if (!g_strcmp0(interface, "wl_subcompositor"))
        probe->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    else if (!g_strcmp0(interface, "zwp_linux_dmabuf_v1")) {
        probe->dmabuf_version = MIN(version, 3);
        probe->dmabuf = wl_registry_bind(
            registry,
            name,
            &zwp_linux_dmabuf_v1_interface,
            probe->dmabuf_version
        );
        if (probe->dmabuf_version >= 3)
            zwp_linux_dmabuf_v1_add_listener(probe->dmabuf, &dmabuf_listener, probe);
    }
}

static void subsurface_probe_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener subsurface_probe_registry_listener = {
    .global = subsurface_probe_global,
    .global_remove = subsurface_probe_global_remove,
};

static gboolean run_live_subsurface_view(
    struct wl_display *display,
    struct wl_event_queue *queue,
    SubsurfaceProbe *probe,
    struct wl_surface *surface,
    char **error_message
);

int fjord_wayland_subsurface_probe(void *display_ptr, void *parent_surface_ptr, char **error_message) {
    struct wl_display *display = display_ptr;
    struct wl_surface *parent_surface = parent_surface_ptr;
    struct wl_registry *registry = NULL;
    struct wl_event_queue *queue = NULL;
    struct wl_proxy *display_wrapper = NULL;
    struct wl_surface *surface = NULL;
    struct wl_subsurface *subsurface = NULL;
    struct wl_region *empty_input_region = NULL;
    SubsurfaceProbe probe = { 0 };

    g_return_val_if_fail(display, 1);
    g_return_val_if_fail(parent_surface, 1);
    g_return_val_if_fail(error_message, 1);

    *error_message = NULL;
    queue = wl_display_create_queue(display);
    display_wrapper = wl_proxy_create_wrapper(display);
    if (!queue || !display_wrapper) {
        *error_message = g_strdup("failed to create Wayland subsurface event queue");
        goto out;
    }
    wl_proxy_set_queue(display_wrapper, queue);
    registry = wl_display_get_registry((struct wl_display *)display_wrapper);
    wl_proxy_wrapper_destroy(display_wrapper);
    display_wrapper = NULL;
    wl_registry_add_listener(registry, &subsurface_probe_registry_listener, &probe);
    if (wl_display_roundtrip_queue(display, queue) < 0 || !probe.compositor || !probe.subcompositor || !probe.dmabuf) {
        *error_message = g_strdup("Wayland compositor does not support dma-buf subsurfaces");
        goto out;
    }
    if (probe.dmabuf_version < 3) {
        *error_message = g_strdup("Wayland compositor lacks dma-buf modifier support");
        goto out;
    }
    if (wl_display_roundtrip_queue(display, queue) < 0) {
        *error_message = g_strdup("failed to query Wayland dma-buf modifiers");
        goto out;
    }

    surface = wl_compositor_create_surface(probe.compositor);
    subsurface = wl_subcompositor_get_subsurface(probe.subcompositor, surface, parent_surface);
    empty_input_region = wl_compositor_create_region(probe.compositor);
    if (!surface || !subsurface || !empty_input_region) {
        *error_message = g_strdup("failed to create Wayland subsurface");
        goto out;
    }
    wl_surface_set_input_region(surface, empty_input_region);
    wl_subsurface_set_desync(subsurface);

    if (!run_live_subsurface_view(display, queue, &probe, surface, error_message))
        goto out;

out:
    if (display_wrapper)
        wl_proxy_wrapper_destroy(display_wrapper);
    if (empty_input_region)
        wl_region_destroy(empty_input_region);
    if (subsurface)
        wl_subsurface_destroy(subsurface);
    if (surface)
        wl_surface_destroy(surface);
    if (probe.dmabuf)
        zwp_linux_dmabuf_v1_destroy(probe.dmabuf);
    if (probe.subcompositor)
        wl_subcompositor_destroy(probe.subcompositor);
    if (probe.compositor)
        wl_compositor_destroy(probe.compositor);
    if (registry)
        wl_registry_destroy(registry);
    wl_display_flush(display);
    if (queue)
        wl_event_queue_destroy(queue);

    return *error_message ? 1 : 0;
}

static void capture_buffer(SmokeState *state, WPEBuffer *buffer) {
    FjordWpeSmokeReport *report = state->report;

    if (report->width)
        return;

    report->width = (uint32_t)wpe_buffer_get_width(buffer);
    report->height = (uint32_t)wpe_buffer_get_height(buffer);
    report->explicit_sync = wpe_display_use_explicit_sync(wpe_buffer_get_display(buffer));

    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        WPEBufferDMABuf *dma_buf = WPE_BUFFER_DMA_BUF(buffer);
        g_strlcpy(report->buffer_kind, "dma-buf", sizeof(report->buffer_kind));
        report->format = wpe_buffer_dma_buf_get_format(dma_buf);
        report->modifier = wpe_buffer_dma_buf_get_modifier(dma_buf);
        report->planes = wpe_buffer_dma_buf_get_n_planes(dma_buf);
        for (guint plane = 0; plane < report->planes; plane++) {
            int fd = wpe_buffer_dma_buf_get_fd(dma_buf, plane);

            if (plane >= G_N_ELEMENTS(report->dma_buf_fds)) {
                set_error(state, "WPE dma-buf has too many planes");
                return;
            }
            if (fd < 0 || (report->dma_buf_fds[plane] = fcntl(fd, F_DUPFD_CLOEXEC, 3)) < 0) {
                set_error(state, "failed to duplicate WPE dma-buf plane");
                return;
            }
            report->offsets[plane] = wpe_buffer_dma_buf_get_offset(dma_buf, plane);
            report->strides[plane] = wpe_buffer_dma_buf_get_stride(dma_buf, plane);
        }
        report->stride = report->strides[0];
    } else if (WPE_IS_BUFFER_SHM(buffer)) {
        WPEBufferSHM *shm = WPE_BUFFER_SHM(buffer);
        g_strlcpy(report->buffer_kind, "shm", sizeof(report->buffer_kind));
        report->format = wpe_buffer_shm_get_format(shm);
        report->stride = wpe_buffer_shm_get_stride(shm);
        report->planes = 1;
    } else {
        g_strlcpy(report->buffer_kind, "unknown", sizeof(report->buffer_kind));
    }
}

static void capture_display_capabilities(FjordWpeSmokeReport *report, WPEDisplay *display) {
    WPEDRMDevice *device = wpe_display_get_drm_device(display);
    WPEBufferFormats *formats = wpe_display_get_preferred_buffer_formats(display);

    report->platform_major = wpe_platform_get_major_version();
    report->platform_minor = wpe_platform_get_minor_version();
    report->platform_micro = wpe_platform_get_micro_version();
    if (device) {
        const char *primary_node = wpe_drm_device_get_primary_node(device);
        const char *render_node = wpe_drm_device_get_render_node(device);
        if (primary_node)
            g_strlcpy(report->primary_node, primary_node, sizeof(report->primary_node));
        if (render_node)
            g_strlcpy(report->render_node, render_node, sizeof(report->render_node));
    }
    if (!formats)
        return;

    for (guint group = 0; group < wpe_buffer_formats_get_n_groups(formats); group++) {
        guint count = wpe_buffer_formats_get_group_n_formats(formats, group);
        report->preferred_format_count += count;
        if (!count || report->dma_buf_advertised)
            continue;

        report->dma_buf_advertised = true;
        report->preferred_format = wpe_buffer_formats_get_format_fourcc(formats, group, 0);
        GArray *modifiers = wpe_buffer_formats_get_format_modifiers(formats, group, 0);
        if (modifiers && modifiers->len)
            report->preferred_modifier = g_array_index(modifiers, guint64, 0);
    }
}

static gboolean verify_egl_import(
    WPEDisplay *wpe_display,
    FjordWpeSmokeReport *report,
    char **error_message
) {
    EGLDisplay display;
    EGLImageKHR image;
    EGLint attributes[48];
    GError *error = NULL;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;
    guint attribute = 0;

    if (g_strcmp0(report->buffer_kind, "dma-buf"))
        return TRUE;

    display = wpe_display_get_egl_display(wpe_display, &error);
    if (!display) {
        *error_message = g_strdup(error ? error->message : "failed to get WPE EGL display");
        g_clear_error(&error);
        return FALSE;
    }
    create_image = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (!create_image || !destroy_image) {
        *error_message = g_strdup("EGL dma-buf import functions are unavailable");
        return FALSE;
    }

    attributes[attribute++] = EGL_WIDTH;
    attributes[attribute++] = report->width;
    attributes[attribute++] = EGL_HEIGHT;
    attributes[attribute++] = report->height;
    attributes[attribute++] = EGL_LINUX_DRM_FOURCC_EXT;
    attributes[attribute++] = report->format;
    for (guint plane = 0; plane < report->planes; plane++) {
        static const EGLint fd_attributes[] = {
            EGL_DMA_BUF_PLANE0_FD_EXT,
            EGL_DMA_BUF_PLANE1_FD_EXT,
            EGL_DMA_BUF_PLANE2_FD_EXT,
            EGL_DMA_BUF_PLANE3_FD_EXT,
        };
        static const EGLint offset_attributes[] = {
            EGL_DMA_BUF_PLANE0_OFFSET_EXT,
            EGL_DMA_BUF_PLANE1_OFFSET_EXT,
            EGL_DMA_BUF_PLANE2_OFFSET_EXT,
            EGL_DMA_BUF_PLANE3_OFFSET_EXT,
        };
        static const EGLint pitch_attributes[] = {
            EGL_DMA_BUF_PLANE0_PITCH_EXT,
            EGL_DMA_BUF_PLANE1_PITCH_EXT,
            EGL_DMA_BUF_PLANE2_PITCH_EXT,
            EGL_DMA_BUF_PLANE3_PITCH_EXT,
        };
        static const EGLint modifier_lo_attributes[] = {
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
            EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
            EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
            EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
        };
        static const EGLint modifier_hi_attributes[] = {
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
            EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
            EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
            EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
        };

        if (plane >= G_N_ELEMENTS(fd_attributes) || report->dma_buf_fds[plane] < 0) {
            *error_message = g_strdup("WPE dma-buf plane is unavailable for EGL import");
            return FALSE;
        }
        attributes[attribute++] = fd_attributes[plane];
        attributes[attribute++] = report->dma_buf_fds[plane];
        attributes[attribute++] = offset_attributes[plane];
        attributes[attribute++] = report->offsets[plane];
        attributes[attribute++] = pitch_attributes[plane];
        attributes[attribute++] = report->strides[plane];
        attributes[attribute++] = modifier_lo_attributes[plane];
        attributes[attribute++] = report->modifier & G_MAXUINT32;
        attributes[attribute++] = modifier_hi_attributes[plane];
        attributes[attribute++] = report->modifier >> 32;
    }
    attributes[attribute++] = EGL_NONE;
    image = create_image(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attributes);
    if (image == EGL_NO_IMAGE_KHR) {
        *error_message = g_strdup("EGL rejected WPE dma-buf import");
        return FALSE;
    }
    if (!destroy_image(display, image)) {
        *error_message = g_strdup("failed to destroy imported EGL image");
        return FALSE;
    }
    report->egl_imported = true;
    return TRUE;
}

static gboolean is_descendant_of_self(long process_id) {
    long parent = process_id;

    for (guint depth = 0; depth < 64 && parent > 1; depth++) {
        FILE *status;
        char status_path[64];
        char line[64];

        if (parent == getpid())
            return TRUE;
        g_snprintf(status_path, sizeof(status_path), "/proc/%ld/status", parent);
        status = fopen(status_path, "r");
        if (!status)
            return FALSE;
        parent = 0;
        while (fgets(line, sizeof(line), status)) {
            if (g_str_has_prefix(line, "PPid:")) {
                parent = strtol(line + 5, NULL, 10);
                break;
            }
        }
        fclose(status);
    }

    return FALSE;
}

static gboolean sandboxed_descendant_count(uint32_t *count) {
    char self_namespace[PATH_MAX];
    ssize_t self_length;
    DIR *proc;
    struct dirent *entry;

    self_length = readlink("/proc/self/ns/user", self_namespace, sizeof(self_namespace) - 1);
    if (self_length < 0)
        return FALSE;
    self_namespace[self_length] = '\0';
    *count = 0;

    proc = opendir("/proc");
    if (!proc)
        return FALSE;

    while ((entry = readdir(proc))) {
        char *end = NULL;
        long value = strtol(entry->d_name, &end, 10);
        char namespace_path[64];
        char namespace[PATH_MAX];
        ssize_t namespace_length;

        if (!entry->d_name[0] || *end || value <= 1)
            continue;
        if (!is_descendant_of_self(value))
            continue;

        g_snprintf(namespace_path, sizeof(namespace_path), "/proc/%ld/ns/user", value);
        namespace_length = readlink(namespace_path, namespace, sizeof(namespace) - 1);
        if (namespace_length < 0)
            continue;
        namespace[namespace_length] = '\0';
        if (g_strcmp0(self_namespace, namespace) != 0)
            (*count)++;
    }

    closedir(proc);
    return TRUE;
}

static gboolean has_sandboxed_descendant(void) {
    uint32_t count;

    return sandboxed_descendant_count(&count) && count;
}

static void maybe_terminate(SmokeState *state) {
    FjordWpeSmokeReport *report = state->report;

    if (state->termination_requested || !report->committed || !report->finished ||
        !report->title_changed || !report->buffer_rendered || !report->buffer_released)
        return;

    state->termination_requested = TRUE;
    report->sandbox_verified = has_sandboxed_descendant();
    if (!report->sandbox_verified)
        set_error(state, "no WPE descendant entered a separate user namespace");
    if (state->terminate_web_process)
        webkit_web_view_terminate_web_process(state->web_view);
    else
        g_main_loop_quit(state->loop);
}

static void load_changed(WebKitWebView *web_view, WebKitLoadEvent event, SmokeState *state) {
    if (event == WEBKIT_LOAD_COMMITTED)
        state->report->committed = true;
    if (event == WEBKIT_LOAD_FINISHED) {
        state->report->finished = true;
        if (state->expected_uri &&
            g_strcmp0(webkit_web_view_get_uri(web_view), state->expected_uri) != 0)
            set_error(state, "WPE did not finish at the requested URI");
    }
    maybe_terminate(state);
}

static gboolean load_failed(
    WebKitWebView *web_view,
    WebKitLoadEvent event,
    const char *failing_uri,
    GError *error,
    SmokeState *state
) {
    (void)web_view;
    (void)event;
    (void)failing_uri;
    (void)error;

    set_error(state, "WPE page load failed");
    g_main_loop_quit(state->loop);
    return FALSE;
}

static void title_changed(WebKitWebView *web_view, GParamSpec *spec, SmokeState *state) {
    const char *title;

    (void)spec;
    title = webkit_web_view_get_title(web_view);
    if (title && (!state->expected_title || g_strcmp0(title, state->expected_title) == 0))
        state->report->title_changed = true;
    maybe_terminate(state);
}

static void uri_changed(WebKitWebView *web_view, GParamSpec *spec, SmokeState *state) {
    (void)spec;

    if (webkit_web_view_get_uri(web_view))
        state->report->uri_changed = true;
}

static void buffers_changed(WPEView *view, WPEBuffer **buffers, guint count, SmokeState *state) {
    (void)view;

    state->report->buffers_changed = true;
    if (count)
        capture_buffer(state, buffers[0]);
}

static void buffer_rendered(WPEView *view, WPEBuffer *buffer, SmokeState *state) {
    (void)view;

    state->report->buffer_rendered = true;
    capture_buffer(state, buffer);
    maybe_terminate(state);
}

static void buffer_released(WPEView *view, WPEBuffer *buffer, SmokeState *state) {
    (void)view;
    (void)buffer;

    state->report->buffer_released = true;
    maybe_terminate(state);
}

static void web_process_terminated(
    WebKitWebView *web_view,
    WebKitWebProcessTerminationReason reason,
    SmokeState *state
) {
    (void)web_view;

    state->report->web_process_terminated = true;
    state->report->termination_reason = reason;
    if (!state->termination_requested)
        set_error(state, "WPE web process terminated before lifecycle verification");
    else if (reason != WEBKIT_WEB_PROCESS_TERMINATED_BY_API)
        set_error(state, "WPE web process did not terminate through the requested API path");
    g_main_loop_quit(state->loop);
}

static gboolean timeout_cb(SmokeState *state) {
    set_error(state, "timed out waiting for WPE lifecycle events");
    g_main_loop_quit(state->loop);
    return G_SOURCE_REMOVE;
}

static GSource *attach_timeout(GMainContext *context, guint milliseconds, GSourceFunc callback, gpointer data) {
    GSource *source = g_timeout_source_new(milliseconds);

    g_source_set_callback(source, callback, data, NULL);
    g_source_attach(source, context);
    return source;
}

static gboolean run_view(
    GMainContext *context,
    GMainLoop *loop,
    WPEDisplay *display,
    WebKitWebContext *web_context,
    WebKitNetworkSession *network_session,
    const char *uri,
    gboolean terminate_web_process,
    FjordWpeSmokeReport *report,
    char **error_message
) {
    static const char fixture[] =
        "<!doctype html><title>loading</title><body>fjord</body>"
        "<script>document.title='Fjord WPE smoke';"
        "setTimeout(() => document.body.style.background='#111', 50);</script>";
    SmokeState state = {0};
    WPEView *view = NULL;
    WPEToplevel *toplevel = NULL;
    GSource *timeout = NULL;

    state.report = report;
    state.expected_title = uri ? NULL : "Fjord WPE smoke";
    state.expected_uri = uri;
    state.terminate_web_process = terminate_web_process;
    state.context = context;
    state.loop = loop;

    state.web_view = WEBKIT_WEB_VIEW(g_object_new(
        WEBKIT_TYPE_WEB_VIEW,
        "display", display,
        "web-context", web_context,
        "network-session", network_session,
        NULL
    ));
    if (!state.web_view) {
        set_error(&state, "failed to create WPE web view");
        goto out;
    }

    view = webkit_web_view_get_wpe_view(state.web_view);
    toplevel = view ? wpe_view_get_toplevel(view) : NULL;
    if (!view || !toplevel || !wpe_toplevel_resize(toplevel, 800, 600)) {
        set_error(&state, "failed to configure WPE headless view");
        goto out;
    }

    g_signal_connect(state.web_view, "load-changed", G_CALLBACK(load_changed), &state);
    g_signal_connect(state.web_view, "load-failed", G_CALLBACK(load_failed), &state);
    g_signal_connect(state.web_view, "notify::title", G_CALLBACK(title_changed), &state);
    g_signal_connect(state.web_view, "notify::uri", G_CALLBACK(uri_changed), &state);
    g_signal_connect(state.web_view, "web-process-terminated", G_CALLBACK(web_process_terminated), &state);
    g_signal_connect(view, "buffers-changed", G_CALLBACK(buffers_changed), &state);
    g_signal_connect(view, "buffer-rendered", G_CALLBACK(buffer_rendered), &state);
    g_signal_connect(view, "buffer-released", G_CALLBACK(buffer_released), &state);

    timeout = attach_timeout(state.context, 15000, G_SOURCE_FUNC(timeout_cb), &state);
    if (uri)
        webkit_web_view_load_uri(state.web_view, uri);
    else
        webkit_web_view_load_html(state.web_view, fixture, "fjord-smoke://fixture/");
    g_main_loop_run(state.loop);
    g_source_destroy(timeout);
    g_source_unref(timeout);
    timeout = NULL;

    if (!state.error_message &&
        (!report->committed || !report->finished || !report->title_changed || !report->uri_changed ||
         !report->buffers_changed || !report->buffer_rendered || !report->buffer_released ||
         (terminate_web_process && !report->web_process_terminated)))
        set_error(&state, "WPE lifecycle completed without all required events");

out:
    if (timeout) {
        g_source_destroy(timeout);
        g_source_unref(timeout);
    }
    if (view)
        g_signal_handlers_disconnect_by_data(view, &state);
    if (state.web_view)
        g_signal_handlers_disconnect_by_data(state.web_view, &state);
    if (state.web_view)
        g_object_unref(state.web_view);

    if (state.error_message) {
        *error_message = state.error_message;
        return FALSE;
    }

    return TRUE;
}

static void live_buffer_fail(LiveBuffer *live, const char *message) {
    if (!*live->error_message)
        *live->error_message = g_strdup(message);
    g_main_loop_quit(live->loop);
}

static gboolean attach_live_buffer(LiveBuffer *live, WPEBuffer *wpe_buffer) {
    WPEBufferDMABuf *dma_buf;
    struct zwp_linux_buffer_params_v1 *params = NULL;
    BufferCreation creation = { 0 };
    int plane_fds[4] = { -1, -1, -1, -1 };
    guint planes;

    if (!WPE_IS_BUFFER_DMA_BUF(wpe_buffer)) {
        live_buffer_fail(live, "WPE headless view did not render a dma-buf");
        return FALSE;
    }
    dma_buf = WPE_BUFFER_DMA_BUF(wpe_buffer);
    planes = wpe_buffer_dma_buf_get_n_planes(dma_buf);
    if (!planes || planes > G_N_ELEMENTS(plane_fds)) {
        live_buffer_fail(live, "WPE dma-buf has an unsupported plane count");
        return FALSE;
    }

    params = zwp_linux_dmabuf_v1_create_params(live->dmabuf);
    if (!params) {
        live_buffer_fail(live, "failed to create Wayland dma-buf parameters");
        goto out;
    }
    for (guint plane = 0; plane < planes; plane++) {
        int fd = wpe_buffer_dma_buf_get_fd(dma_buf, plane);

        plane_fds[plane] = fd >= 0 ? fcntl(fd, F_DUPFD_CLOEXEC, 3) : -1;
        if (plane_fds[plane] < 0) {
            live_buffer_fail(live, "failed to duplicate a WPE dma-buf plane");
            goto out;
        }
        zwp_linux_buffer_params_v1_add(
            params,
            plane_fds[plane],
            plane,
            wpe_buffer_dma_buf_get_offset(dma_buf, plane),
            wpe_buffer_dma_buf_get_stride(dma_buf, plane),
            wpe_buffer_dma_buf_get_modifier(dma_buf) >> 32,
            wpe_buffer_dma_buf_get_modifier(dma_buf) & G_MAXUINT32
        );
    }
    zwp_linux_buffer_params_v1_add_listener(params, &buffer_creation_listener, &creation);
    zwp_linux_buffer_params_v1_create(
        params,
        wpe_buffer_get_width(wpe_buffer),
        wpe_buffer_get_height(wpe_buffer),
        wpe_buffer_dma_buf_get_format(dma_buf),
        0
    );
    if (wl_display_roundtrip_queue(live->display, live->queue) < 0) {
        live_buffer_fail(live, "Wayland connection failed while creating the WPE dma-buf");
        goto out;
    }
    for (guint plane = 0; plane < planes; plane++)
        plane_fds[plane] = -1; // libwayland closes the duplicated descriptors after flushing them.
    if (creation.failed || !creation.buffer) {
        live_buffer_fail(live, "Wayland compositor rejected the WPE dma-buf");
        goto out;
    }

    live->buffer = creation.buffer;
    live->wpe_buffer = wpe_buffer;
    live->in_flight = TRUE;
    wl_buffer_add_listener(live->buffer, &live_buffer_listener, live);
    wl_surface_attach(live->surface, live->buffer, 0, 0);
    wl_surface_damage(live->surface, 0, 0, wpe_buffer_get_width(wpe_buffer), wpe_buffer_get_height(wpe_buffer));
    wl_surface_commit(live->surface);
    if (wl_display_roundtrip_queue(live->display, live->queue) < 0) {
        live_buffer_fail(live, "failed to present the WPE dma-buf");
        goto out;
    }
    wl_surface_attach(live->surface, NULL, 0, 0);
    wl_surface_commit(live->surface);
    if (wl_display_flush(live->display) < 0) {
        live_buffer_fail(live, "failed to detach the WPE dma-buf");
        goto out;
    }
    for (guint attempt = 0; attempt < 2 && live->in_flight; attempt++) {
        if (wl_display_roundtrip_queue(live->display, live->queue) < 0) {
            live_buffer_fail(live, "failed to dispatch the WPE dma-buf release");
            goto out;
        }
    }
    if (live->in_flight)
        live_buffer_fail(live, "Wayland compositor did not release the WPE dma-buf");

out:
    if (params)
        zwp_linux_buffer_params_v1_destroy(params);
    for (guint plane = 0; plane < G_N_ELEMENTS(plane_fds); plane++) {
        if (plane_fds[plane] >= 0)
            close(plane_fds[plane]);
    }
    return !*live->error_message;
}

static void live_buffer_rendered(WPEView *view, WPEBuffer *buffer, LiveBuffer *live) {
    if (live->index != live->smoke->active_view) {
        wpe_view_buffer_released(view, buffer);
    } else if (!live->buffer) {
        live->view = view;
        attach_live_buffer(live, buffer);
    }
}

static gboolean live_buffer_timeout(LiveBuffer *live) {
    live_buffer_fail(live, "timed out waiting for Wayland to release the WPE dma-buf");
    return G_SOURCE_REMOVE;
}

static void remove_tree(const char *path) {
    GDir *directory = g_dir_open(path, 0, NULL);
    const char *name;

    if (!directory) {
        g_remove(path);
        return;
    }
    while ((name = g_dir_read_name(directory))) {
        char *child = g_build_filename(path, name, NULL);

        remove_tree(child);
        g_free(child);
    }
    g_dir_close(directory);
    g_rmdir(path);
}

static gboolean run_live_subsurface_view(
    struct wl_display *wayland_display,
    struct wl_event_queue *queue,
    SubsurfaceProbe *probe,
    struct wl_surface *surface,
    char **error_message
) {
    static const char *fixtures[] = {
        "<!doctype html><title>Fjord Gate 2 Crimson</title><body>crimson</body>"
        "<script>let n = 0; setInterval(() => document.body.style.background = n++ % 2 ? '#9b1c31' : '#ef4444', 100);</script>",
        "<!doctype html><title>Fjord Gate 2 Azure</title><body>azure</body>"
        "<script>let n = 0; setInterval(() => document.body.style.background = n++ % 2 ? '#1d4ed8' : '#38bdf8', 100);</script>",
    };
    GMainContext *context = NULL;
    GMainLoop *loop = NULL;
    WPEDisplay *display = NULL;
    WebKitWebContext *web_context = NULL;
    WebKitNetworkSession *network_session = NULL;
    WebKitWebView *web_views[2] = { NULL, NULL };
    WPEView *views[2] = { NULL, NULL };
    WPEToplevel *toplevels[2] = { NULL, NULL };
    GSource *timeout = NULL;
    GError *directory_error = NULL;
    char *profile = NULL;
    char *data_directory = NULL;
    char *cache_directory = NULL;
    LiveSmoke smoke = {
        .error_message = error_message,
        .target_frames = 20,
        .target_resizes = 19,
    };
    LiveBuffer live[2] = { 0 };

    context = g_main_context_new();
    g_main_context_push_thread_default(context);
    loop = g_main_loop_new(context, FALSE);
    smoke.loop = loop;
    profile = g_dir_make_tmp("fjord-gate2-XXXXXX", &directory_error);
    if (!profile) {
        *error_message = g_strdup(directory_error->message);
        g_clear_error(&directory_error);
        goto out;
    }
    data_directory = g_build_filename(profile, "data", NULL);
    cache_directory = g_build_filename(profile, "cache", NULL);
    if (g_mkdir(data_directory, 0700) || g_mkdir(cache_directory, 0700)) {
        *error_message = g_strdup("failed to create WPE smoke profile directories");
        goto out;
    }
    display = wpe_display_headless_new();
    web_context = webkit_web_context_new();
    network_session = webkit_network_session_new(data_directory, cache_directory);
    if (!display || !web_context || !network_session) {
        *error_message = g_strdup("failed to create WPE headless smoke view");
        goto out;
    }
    for (guint index = 0; index < G_N_ELEMENTS(live); index++) {
        live[index] = (LiveBuffer) {
            .display = wayland_display,
            .queue = queue,
            .dmabuf = probe->dmabuf,
            .surface = surface,
            .loop = loop,
            .error_message = error_message,
            .smoke = &smoke,
            .index = index,
        };
        web_views[index] = WEBKIT_WEB_VIEW(g_object_new(
            WEBKIT_TYPE_WEB_VIEW,
            "display", display,
            "web-context", web_context,
            "network-session", network_session,
            NULL
        ));
        views[index] = web_views[index] ? webkit_web_view_get_wpe_view(web_views[index]) : NULL;
        toplevels[index] = views[index] ? wpe_view_get_toplevel(views[index]) : NULL;
        if (!views[index] || !toplevels[index] || !wpe_toplevel_resize(toplevels[index], 800, 600)) {
            *error_message = g_strdup("failed to configure WPE headless smoke view");
            goto out;
        }
        live[index].toplevel = toplevels[index];
        g_signal_connect(views[index], "buffer-rendered", G_CALLBACK(live_buffer_rendered), &live[index]);
    }
    timeout = attach_timeout(context, 15000, G_SOURCE_FUNC(live_buffer_timeout), &live[0]);
    for (guint index = 0; index < G_N_ELEMENTS(live); index++)
        webkit_web_view_load_html(web_views[index], fixtures[index], "fjord-gate2://fixture/");
    g_main_loop_run(loop);
    if (!*error_message &&
        (smoke.released_frames != smoke.target_frames || smoke.resize_count != smoke.target_resizes))
        *error_message = g_strdup("WPE live subsurface did not finish every resize frame");

out:
    if (timeout) {
        g_source_destroy(timeout);
        g_source_unref(timeout);
    }
    for (guint index = 0; index < G_N_ELEMENTS(live); index++) {
        if (views[index])
            g_signal_handlers_disconnect_by_data(views[index], &live[index]);
        if (live[index].buffer) {
            wl_proxy_set_user_data((struct wl_proxy *)live[index].buffer, NULL);
            wl_buffer_destroy(live[index].buffer);
            wl_display_flush(wayland_display);
        }
    }
    for (guint index = 0; index < G_N_ELEMENTS(web_views); index++) {
        if (web_views[index])
            g_object_unref(web_views[index]);
    }
    if (network_session)
        g_object_unref(network_session);
    if (web_context)
        g_object_unref(web_context);
    if (display)
        g_object_unref(display);
    if (profile)
        remove_tree(profile);
    g_free(cache_directory);
    g_free(data_directory);
    g_free(profile);
    if (loop)
        g_main_loop_unref(loop);
    if (context) {
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);
    }
    return !*error_message;
}

typedef struct FjordWpeSubsurfaceBridge FjordWpeSubsurfaceBridge;

typedef struct {
    FjordWpeSubsurfaceBridge *bridge;
    struct zwp_linux_buffer_params_v1 *params;
    struct wl_buffer *buffer;
    WPEBuffer *wpe_buffer;
    WPEBuffer *pending_wpe_buffer;
    int plane_fds[4];
    gboolean in_flight;
} BridgeBuffer;

struct FjordWpeSubsurfaceBridge {
    struct wl_display *wayland_display;
    struct wl_surface *parent_surface;
    struct wl_event_queue *queue;
    struct wl_registry *registry;
    SubsurfaceProbe probe;
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    struct wl_region *empty_input_region;
    GMainContext *context;
    WPEDisplay *wpe_display;
    WebKitWebContext *web_context;
    WebKitNetworkSession *network_session;
    WebKitWebView *web_view;
    WPEView *view;
    WPEToplevel *toplevel;
    char *profile;
    char *data_directory;
    char *cache_directory;
    char *error_message;
    BridgeBuffer frame;
};

static gboolean bridge_create_buffer(FjordWpeSubsurfaceBridge *bridge, WPEBuffer *wpe_buffer);

static void bridge_fail(FjordWpeSubsurfaceBridge *bridge, const char *message) {
    if (!bridge->error_message)
        bridge->error_message = g_strdup(message);
}

static void bridge_release_buffer(BridgeBuffer *frame) {
    if (!frame->wpe_buffer)
        return;
    wpe_view_buffer_released(frame->bridge->view, frame->wpe_buffer);
    frame->wpe_buffer = NULL;
}

static void bridge_detach_buffer(BridgeBuffer *frame) {
    FjordWpeSubsurfaceBridge *bridge = frame->bridge;

    wl_surface_attach(bridge->surface, NULL, 0, 0);
    wl_surface_commit(bridge->surface);
}

static void bridge_buffer_release(void *data, struct wl_buffer *buffer) {
    BridgeBuffer *frame = data;

    if (!frame || frame->buffer != buffer || !frame->in_flight)
        return;
    frame->in_flight = FALSE;
    wl_buffer_destroy(frame->buffer);
    frame->buffer = NULL;
    bridge_release_buffer(frame);
    if (frame->pending_wpe_buffer) {
        WPEBuffer *pending = frame->pending_wpe_buffer;

        frame->pending_wpe_buffer = NULL;
        bridge_create_buffer(frame->bridge, pending);
    }
}

static const struct wl_buffer_listener bridge_buffer_listener = {
    .release = bridge_buffer_release,
};

static void bridge_attach_buffer(BridgeBuffer *frame) {
    FjordWpeSubsurfaceBridge *bridge = frame->bridge;
    WPEBuffer *wpe_buffer = frame->wpe_buffer;

    if (!frame->buffer || !wpe_buffer) {
        bridge_fail(bridge, "Wayland compositor created an invalid WPE dma-buf");
        return;
    }
    frame->in_flight = TRUE;
    wl_buffer_add_listener(frame->buffer, &bridge_buffer_listener, frame);
    wl_surface_attach(bridge->surface, frame->buffer, 0, 0);
    wl_surface_damage(
        bridge->surface,
        0,
        0,
        wpe_buffer_get_width(wpe_buffer),
        wpe_buffer_get_height(wpe_buffer)
    );
    wl_surface_commit(bridge->surface);
    // Keep the current frame visible until a later frame replaces it.
}

static void bridge_buffer_created(
    void *data,
    struct zwp_linux_buffer_params_v1 *params,
    struct wl_buffer *buffer
) {
    BridgeBuffer *frame = data;

    (void)params;
    frame->buffer = buffer;
    if (frame->params) {
        zwp_linux_buffer_params_v1_destroy(frame->params);
        frame->params = NULL;
    }
    bridge_attach_buffer(frame);
}

static void bridge_buffer_creation_failed(void *data, struct zwp_linux_buffer_params_v1 *params) {
    BridgeBuffer *frame = data;

    (void)params;
    if (frame->params) {
        zwp_linux_buffer_params_v1_destroy(frame->params);
        frame->params = NULL;
    }
    bridge_fail(frame->bridge, "Wayland compositor rejected the WPE dma-buf");
    bridge_release_buffer(frame);
}

static const struct zwp_linux_buffer_params_v1_listener bridge_buffer_creation_listener = {
    .created = bridge_buffer_created,
    .failed = bridge_buffer_creation_failed,
};

static gboolean bridge_create_buffer(FjordWpeSubsurfaceBridge *bridge, WPEBuffer *wpe_buffer) {
    BridgeBuffer *frame = &bridge->frame;
    WPEBufferDMABuf *dma_buf;
    guint planes;

    if (!WPE_IS_BUFFER_DMA_BUF(wpe_buffer)) {
        bridge_fail(bridge, "WPE headless view did not render a dma-buf");
        return FALSE;
    }
    dma_buf = WPE_BUFFER_DMA_BUF(wpe_buffer);
    planes = wpe_buffer_dma_buf_get_n_planes(dma_buf);
    if (!planes || planes > G_N_ELEMENTS(frame->plane_fds)) {
        bridge_fail(bridge, "WPE dma-buf has an unsupported plane count");
        return FALSE;
    }

    frame->wpe_buffer = wpe_buffer;
    frame->params = zwp_linux_dmabuf_v1_create_params(bridge->probe.dmabuf);
    if (!frame->params) {
        bridge_fail(bridge, "failed to create Wayland dma-buf parameters");
        goto failed;
    }
    for (guint plane = 0; plane < planes; plane++) {
        int fd = wpe_buffer_dma_buf_get_fd(dma_buf, plane);

        frame->plane_fds[plane] = fd >= 0 ? fcntl(fd, F_DUPFD_CLOEXEC, 3) : -1;
        if (frame->plane_fds[plane] < 0) {
            bridge_fail(bridge, "failed to duplicate a WPE dma-buf plane");
            goto failed;
        }
        zwp_linux_buffer_params_v1_add(
            frame->params,
            frame->plane_fds[plane],
            plane,
            wpe_buffer_dma_buf_get_offset(dma_buf, plane),
            wpe_buffer_dma_buf_get_stride(dma_buf, plane),
            wpe_buffer_dma_buf_get_modifier(dma_buf) >> 32,
            wpe_buffer_dma_buf_get_modifier(dma_buf) & G_MAXUINT32
        );
    }
    zwp_linux_buffer_params_v1_add_listener(
        frame->params,
        &bridge_buffer_creation_listener,
        frame
    );
    zwp_linux_buffer_params_v1_create(
        frame->params,
        wpe_buffer_get_width(wpe_buffer),
        wpe_buffer_get_height(wpe_buffer),
        wpe_buffer_dma_buf_get_format(dma_buf),
        0
    );
    // libwayland owns these descriptors once the create request is queued.
    for (guint plane = 0; plane < planes; plane++)
        frame->plane_fds[plane] = -1;
    return TRUE;

failed:
    if (frame->params) {
        zwp_linux_buffer_params_v1_destroy(frame->params);
        frame->params = NULL;
    }
    bridge_release_buffer(frame);
    return FALSE;
}

static void bridge_buffer_rendered(
    WPEView *view,
    WPEBuffer *buffer,
    FjordWpeSubsurfaceBridge *bridge
) {
    BridgeBuffer *frame = &bridge->frame;

    (void)view;
    if (bridge->error_message) {
        wpe_view_buffer_released(bridge->view, buffer);
    } else if (!frame->wpe_buffer) {
        bridge_create_buffer(bridge, buffer);
    } else if (frame->pending_wpe_buffer) {
        wpe_view_buffer_released(bridge->view, frame->pending_wpe_buffer);
        frame->pending_wpe_buffer = buffer;
    } else {
        frame->pending_wpe_buffer = buffer;
        bridge_detach_buffer(frame);
    }
}

static gboolean bridge_start(FjordWpeSubsurfaceBridge *bridge) {
    GError *directory_error = NULL;

    if (bridge->surface)
        return TRUE;
    if (!bridge->probe.compositor || !bridge->probe.subcompositor || !bridge->probe.dmabuf)
        return TRUE;
    if (bridge->probe.dmabuf_version < 3) {
        bridge_fail(bridge, "Wayland compositor lacks dma-buf modifier support");
        return FALSE;
    }

    bridge->surface = wl_compositor_create_surface(bridge->probe.compositor);
    bridge->subsurface = wl_subcompositor_get_subsurface(
        bridge->probe.subcompositor,
        bridge->surface,
        bridge->parent_surface
    );
    bridge->empty_input_region = wl_compositor_create_region(bridge->probe.compositor);
    if (!bridge->surface || !bridge->subsurface || !bridge->empty_input_region) {
        bridge_fail(bridge, "failed to create Wayland subsurface");
        return FALSE;
    }
    wl_surface_set_input_region(bridge->surface, bridge->empty_input_region);
    wl_subsurface_set_desync(bridge->subsurface);

    bridge->profile = g_dir_make_tmp("fjord-gate2-XXXXXX", &directory_error);
    if (!bridge->profile) {
        bridge->error_message = g_strdup(directory_error->message);
        g_clear_error(&directory_error);
        return FALSE;
    }
    bridge->data_directory = g_build_filename(bridge->profile, "data", NULL);
    bridge->cache_directory = g_build_filename(bridge->profile, "cache", NULL);
    if (g_mkdir(bridge->data_directory, 0700) || g_mkdir(bridge->cache_directory, 0700)) {
        bridge_fail(bridge, "failed to create WPE bridge profile directories");
        return FALSE;
    }
    g_main_context_push_thread_default(bridge->context);
    bridge->wpe_display = wpe_display_headless_new();
    bridge->web_context = webkit_web_context_new();
    bridge->network_session = webkit_network_session_new(
        bridge->data_directory,
        bridge->cache_directory
    );
    if (!bridge->wpe_display || !bridge->web_context || !bridge->network_session) {
        bridge_fail(bridge, "failed to create WPE bridge view");
        g_main_context_pop_thread_default(bridge->context);
        return FALSE;
    }
    bridge->web_view = WEBKIT_WEB_VIEW(g_object_new(
        WEBKIT_TYPE_WEB_VIEW,
        "display", bridge->wpe_display,
        "web-context", bridge->web_context,
        "network-session", bridge->network_session,
        NULL
    ));
    bridge->view = bridge->web_view ? webkit_web_view_get_wpe_view(bridge->web_view) : NULL;
    bridge->toplevel = bridge->view ? wpe_view_get_toplevel(bridge->view) : NULL;
    if (!bridge->view || !bridge->toplevel || !wpe_toplevel_resize(bridge->toplevel, 800, 600)) {
        bridge_fail(bridge, "failed to configure WPE bridge view");
        g_main_context_pop_thread_default(bridge->context);
        return FALSE;
    }
    g_signal_connect(bridge->view, "buffer-rendered", G_CALLBACK(bridge_buffer_rendered), bridge);
    webkit_web_view_load_html(
        bridge->web_view,
        "<!doctype html><title>Fjord Input Fixture</title>"
        "<style>body{margin:0;font:16px sans-serif;min-height:2400px;background:#0f172a;color:#e2e8f0}"
        "main{max-width:680px;margin:48px auto;padding:24px;border:1px solid #334155;border-radius:12px;background:#111827}"
        "button,input{font:inherit;padding:10px;margin:8px 0}button{background:#38bdf8;color:#082f49;border:0;border-radius:6px}"
        "#log{white-space:pre-wrap;min-height:120px;padding:12px;background:#020617;border-radius:6px}</style>"
        "<main><h1>Fjord Input Fixture</h1><p>Click, type, use named keys, and scroll this page.</p>"
        "<button id=click>Click target</button><br><input id=text placeholder='Type here' autofocus>"
        "<h2>Event log</h2><div id=log>ready</div></main>"
        "<script>const log=document.querySelector('#log');let n=0;const show=x=>{log.textContent=`${++n}: ${x}\n`+log.textContent;document.title=`Fjord Input Fixture: ${x}`};"
        "document.querySelector('#click').onclick=()=>show('click');document.querySelector('#text').oninput=e=>show(`text ${e.target.value}`);"
        "document.addEventListener('keydown',e=>show(`key ${e.key}`));document.addEventListener('wheel',e=>show(`scroll ${Math.round(e.deltaY)}`));"
        "window.addEventListener('scroll',()=>show(`page ${Math.round(scrollY)}`));</script>",
        "fjord-gate2://bridge/"
    );
    g_main_context_pop_thread_default(bridge->context);
    return TRUE;
}

FjordWpeSubsurfaceBridge *fjord_wpe_subsurface_bridge_new(
    void *display_ptr,
    void *parent_surface_ptr,
    char **error_message
) {
    FjordWpeSubsurfaceBridge *bridge;
    struct wl_proxy *display_wrapper;

    g_return_val_if_fail(display_ptr, NULL);
    g_return_val_if_fail(parent_surface_ptr, NULL);
    g_return_val_if_fail(error_message, NULL);
    *error_message = NULL;

    bridge = g_new0(FjordWpeSubsurfaceBridge, 1);
    bridge->wayland_display = display_ptr;
    bridge->parent_surface = parent_surface_ptr;
    bridge->context = g_main_context_new();
    bridge->frame.bridge = bridge;
    for (guint plane = 0; plane < G_N_ELEMENTS(bridge->frame.plane_fds); plane++)
        bridge->frame.plane_fds[plane] = -1;
    bridge->queue = wl_display_create_queue(bridge->wayland_display);
    display_wrapper = bridge->queue ? wl_proxy_create_wrapper(bridge->wayland_display) : NULL;
    if (!bridge->context || !bridge->queue || !display_wrapper) {
        bridge_fail(bridge, "failed to create Wayland subsurface bridge queue");
        goto failed;
    }
    wl_proxy_set_queue(display_wrapper, bridge->queue);
    bridge->registry = wl_display_get_registry((struct wl_display *)display_wrapper);
    wl_proxy_wrapper_destroy(display_wrapper);
    if (!bridge->registry) {
        bridge_fail(bridge, "failed to create Wayland subsurface bridge registry");
        goto failed;
    }
    wl_registry_add_listener(
        bridge->registry,
        &subsurface_probe_registry_listener,
        &bridge->probe
    );
    if (wl_display_flush(bridge->wayland_display) < 0 && errno != EAGAIN) {
        bridge_fail(bridge, "failed to request Wayland globals for WPE bridge");
        goto failed;
    }
    return bridge;

failed:
    *error_message = bridge->error_message;
    bridge->error_message = NULL;
    fjord_wpe_subsurface_bridge_free(bridge);
    return NULL;
}

int fjord_wpe_subsurface_bridge_pump(FjordWpeSubsurfaceBridge *bridge, char **error_message) {
    g_return_val_if_fail(bridge, 1);
    g_return_val_if_fail(error_message, 1);
    *error_message = NULL;

    while (g_main_context_pending(bridge->context))
        g_main_context_iteration(bridge->context, FALSE);
    if (wl_display_dispatch_queue_pending(bridge->wayland_display, bridge->queue) < 0)
        bridge_fail(bridge, "Wayland connection failed while pumping WPE bridge");
    if (!bridge->error_message)
        bridge_start(bridge);
    if (!bridge->error_message && wl_display_flush(bridge->wayland_display) < 0 && errno != EAGAIN)
        bridge_fail(bridge, "failed to flush WPE bridge Wayland requests");
    if (!bridge->error_message)
        return 0;

    *error_message = bridge->error_message;
    bridge->error_message = NULL;
    return 1;
}

int fjord_wpe_subsurface_bridge_pointer_button(
    FjordWpeSubsurfaceBridge *bridge,
    bool pressed,
    double x,
    double y,
    char **error_message
) {
    WPEEvent *event;
    guint32 time;

    g_return_val_if_fail(bridge, 1);
    g_return_val_if_fail(error_message, 1);
    *error_message = NULL;
    if (!bridge->view) {
        *error_message = g_strdup("WPE bridge view is not ready for pointer input");
        return 1;
    }
    if (!isfinite(x) || !isfinite(y)) {
        *error_message = g_strdup("WPE bridge pointer coordinates must be finite");
        return 1;
    }
    time = (guint32)(g_get_monotonic_time() / 1000);

    event = wpe_event_pointer_button_new(
        pressed ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP,
        bridge->view,
        WPE_INPUT_SOURCE_MOUSE,
        time,
        0,
        1,
        x,
        y,
        pressed ? wpe_view_compute_press_count(bridge->view, x, y, 1, time) : 0
    );
    if (!event) {
        *error_message = g_strdup("failed to create WPE pointer button event");
        return 1;
    }
    wpe_view_event(bridge->view, event);
    wpe_event_unref(event);
    return 0;
}

int fjord_wpe_subsurface_bridge_scroll(
    FjordWpeSubsurfaceBridge *bridge,
    double x,
    double y,
    double delta_x,
    double delta_y,
    bool precise,
    char **error_message
) {
    WPEEvent *event;

    g_return_val_if_fail(bridge, 1);
    g_return_val_if_fail(error_message, 1);
    *error_message = NULL;
    if (!bridge->view) {
        *error_message = g_strdup("WPE bridge view is not ready for scroll input");
        return 1;
    }
    if (!isfinite(x) || !isfinite(y) || !isfinite(delta_x) || !isfinite(delta_y)) {
        *error_message = g_strdup("WPE bridge scroll values must be finite");
        return 1;
    }

    event = wpe_event_scroll_new(
        bridge->view,
        WPE_INPUT_SOURCE_MOUSE,
        (guint32)(g_get_monotonic_time() / 1000),
        0,
        delta_x,
        delta_y,
        precise,
        FALSE,
        x,
        y
    );
    if (!event) {
        *error_message = g_strdup("failed to create WPE scroll event");
        return 1;
    }
    wpe_view_event(bridge->view, event);
    wpe_event_unref(event);
    return 0;
}

int fjord_wpe_subsurface_bridge_keyboard(
    FjordWpeSubsurfaceBridge *bridge,
    bool pressed,
    uint32_t keyval,
    char **error_message
) {
    WPEEvent *event;

    g_return_val_if_fail(bridge, 1);
    g_return_val_if_fail(error_message, 1);
    *error_message = NULL;
    if (!bridge->view) {
        *error_message = g_strdup("WPE bridge view is not ready for keyboard input");
        return 1;
    }
    if (!keyval) {
        *error_message = g_strdup("WPE bridge keyboard keyval must not be zero");
        return 1;
    }

    event = wpe_event_keyboard_new(
        pressed ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP,
        bridge->view,
        WPE_INPUT_SOURCE_KEYBOARD,
        (guint32)(g_get_monotonic_time() / 1000),
        0,
        0,
        keyval
    );
    if (!event) {
        *error_message = g_strdup("failed to create WPE keyboard event");
        return 1;
    }
    wpe_view_event(bridge->view, event);
    wpe_event_unref(event);
    return 0;
}

void fjord_wpe_subsurface_bridge_free(FjordWpeSubsurfaceBridge *bridge) {
    if (!bridge)
        return;
    if (bridge->view)
        g_signal_handlers_disconnect_by_data(bridge->view, bridge);
    if (bridge->frame.params)
        zwp_linux_buffer_params_v1_destroy(bridge->frame.params);
    if (bridge->frame.buffer)
        wl_buffer_destroy(bridge->frame.buffer);
    if (bridge->frame.pending_wpe_buffer)
        wpe_view_buffer_released(bridge->view, bridge->frame.pending_wpe_buffer);
    for (guint plane = 0; plane < G_N_ELEMENTS(bridge->frame.plane_fds); plane++) {
        if (bridge->frame.plane_fds[plane] >= 0)
            close(bridge->frame.plane_fds[plane]);
    }
    if (bridge->web_view)
        g_object_unref(bridge->web_view);
    if (bridge->network_session)
        g_object_unref(bridge->network_session);
    if (bridge->web_context)
        g_object_unref(bridge->web_context);
    if (bridge->wpe_display)
        g_object_unref(bridge->wpe_display);
    if (bridge->empty_input_region)
        wl_region_destroy(bridge->empty_input_region);
    if (bridge->subsurface)
        wl_subsurface_destroy(bridge->subsurface);
    if (bridge->surface)
        wl_surface_destroy(bridge->surface);
    if (bridge->probe.dmabuf)
        zwp_linux_dmabuf_v1_destroy(bridge->probe.dmabuf);
    if (bridge->probe.subcompositor)
        wl_subcompositor_destroy(bridge->probe.subcompositor);
    if (bridge->probe.compositor)
        wl_compositor_destroy(bridge->probe.compositor);
    if (bridge->registry)
        wl_registry_destroy(bridge->registry);
    if (bridge->wayland_display)
        wl_display_flush(bridge->wayland_display);
    if (bridge->queue)
        wl_event_queue_destroy(bridge->queue);
    if (bridge->context)
        g_main_context_unref(bridge->context);
    if (bridge->profile)
        remove_tree(bridge->profile);
    g_free(bridge->cache_directory);
    g_free(bridge->data_directory);
    g_free(bridge->profile);
    g_free(bridge->error_message);
    g_free(bridge);
}

static gboolean open_file_descriptor_count(uint32_t *count) {
    DIR *directory = opendir("/proc/self/fd");
    struct dirent *entry;

    if (!directory)
        return FALSE;
    *count = 0;
    while ((entry = readdir(directory))) {
        if (g_strcmp0(entry->d_name, ".") && g_strcmp0(entry->d_name, ".."))
            (*count)++;
    }
    closedir(directory);
    return TRUE;
}

int fjord_wpe_smoke_run(
    const char *data_directory,
    const char *cache_directory,
    const char *uri,
    uint32_t views,
    FjordWpeSmokeReport *report,
    char **error_message
) {
    GMainContext *context = NULL;
    GMainLoop *loop = NULL;
    WPEDisplay *display = NULL;
    WebKitWebContext *web_context = NULL;
    WebKitNetworkSession *network_session = NULL;
    uint32_t fd_baseline = 0;
    uint32_t fd_after = 0;

    g_return_val_if_fail(data_directory, 1);
    g_return_val_if_fail(cache_directory, 1);
    g_return_val_if_fail(views == 1 || views >= 3, 1);
    g_return_val_if_fail(report, 1);
    g_return_val_if_fail(error_message, 1);

    *error_message = NULL;
    memset(report, 0, sizeof(*report));
    for (guint plane = 0; plane < G_N_ELEMENTS(report->dma_buf_fds); plane++)
        report->dma_buf_fds[plane] = -1;
    context = g_main_context_new();
    g_main_context_push_thread_default(context);
    loop = g_main_loop_new(context, FALSE);

    if (g_mkdir_with_parents(data_directory, 0700) || g_mkdir_with_parents(cache_directory, 0700)) {
        *error_message = g_strdup("failed to create WPE profile directories");
        goto out;
    }

    display = wpe_display_headless_new();
    if (!display) {
        *error_message = g_strdup("failed to create WPE display");
        goto out;
    }
    web_context = webkit_web_context_new();
    network_session = webkit_network_session_new(data_directory, cache_directory);
    if (!web_context || !network_session) {
        *error_message = g_strdup("failed to create WPE web context or network session");
        goto out;
    }

    for (uint32_t iteration = 0; iteration < views; iteration++) {
        uint32_t descriptors;
        uint32_t descendants;

        reset_report(report);
        report->sandbox_tools_available =
            g_find_program_in_path("bwrap") != NULL && g_find_program_in_path("xdg-dbus-proxy") != NULL;
        capture_display_capabilities(report, display);
        if (!run_view(
                context,
                loop,
                display,
                web_context,
                network_session,
                uri,
                TRUE,
                report,
                error_message
            ))
            goto out;
        if (!verify_egl_import(display, report, error_message))
            goto out;

        if (views == 1)
            continue;
        // WebKit's Linux memory monitor opens its permanent files after its
        // first five-second poll, so establish the baseline after it starts.
        g_usleep(iteration ? 250000 : 5250000);
        while (g_main_context_pending(context))
            g_main_context_iteration(context, FALSE);
        if (!sandboxed_descendant_count(&descendants)) {
            *error_message = g_strdup("failed to count WPE sandbox descendants");
            goto out;
        }
        if (descendants) {
            *error_message = g_strdup("WPE sandbox descendants remained after view teardown");
            goto out;
        }
        if (!open_file_descriptor_count(&descriptors)) {
            *error_message = g_strdup("failed to count open file descriptors");
            goto out;
        }
        if (iteration == 1)
            fd_baseline = descriptors;
        else if (iteration > 1 && descriptors > fd_baseline) {
            *error_message = g_strdup_printf(
                "WPE view lifecycles increased open file descriptors: %u > %u",
                descriptors,
                fd_baseline
            );
            goto out;
        }
        fd_after = descriptors;
    }
    report->views = views;
    report->fd_baseline = fd_baseline;
    report->fd_after = fd_after;

out:
    if (network_session)
        g_object_unref(network_session);
    if (web_context)
        g_object_unref(web_context);
    if (display)
        g_object_unref(display);
    if (loop)
        g_main_loop_unref(loop);
    g_main_context_pop_thread_default(context);
    g_main_context_unref(context);

    if (*error_message)
        fjord_wpe_smoke_close_fds(report);
    return *error_message ? 1 : 0;
}

void fjord_wpe_smoke_free_error(char *error_message) {
    g_free(error_message);
}

size_t fjord_wpe_smoke_report_size(void) {
    return sizeof(FjordWpeSmokeReport);
}
