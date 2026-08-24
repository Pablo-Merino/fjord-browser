#include "wpe_smoke.h"

#include <dirent.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <glib.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpe/headless/wpe-headless.h>
#include <wpe/webkit.h>
#include <wayland-client-protocol.h>

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
} SubsurfaceProbe;

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

int fjord_wayland_subsurface_probe(void *display_ptr, void *parent_surface_ptr, char **error_message) {
    struct wl_display *display = display_ptr;
    struct wl_surface *parent_surface = parent_surface_ptr;
    struct wl_registry *registry;
    struct wl_surface *surface = NULL;
    struct wl_subsurface *subsurface = NULL;
    struct wl_region *empty_input_region = NULL;
    SubsurfaceProbe probe = { 0 };

    g_return_val_if_fail(display, 1);
    g_return_val_if_fail(parent_surface, 1);
    g_return_val_if_fail(error_message, 1);

    *error_message = NULL;
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &subsurface_probe_registry_listener, &probe);
    if (wl_display_roundtrip(display) < 0 || !probe.compositor || !probe.subcompositor) {
        *error_message = g_strdup("Wayland compositor does not support subsurfaces");
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
    wl_surface_commit(surface);
    wl_surface_commit(parent_surface);
    if (wl_display_flush(display) < 0)
        *error_message = g_strdup("failed to flush Wayland subsurface");

out:
    if (empty_input_region)
        wl_region_destroy(empty_input_region);
    if (subsurface)
        wl_subsurface_destroy(subsurface);
    if (surface)
        wl_surface_destroy(surface);
    if (probe.subcompositor)
        wl_subcompositor_destroy(probe.subcompositor);
    if (probe.compositor)
        wl_compositor_destroy(probe.compositor);
    if (registry)
        wl_registry_destroy(registry);

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

static GSource *attach_timeout(GMainContext *context, guint milliseconds, GSourceFunc callback, SmokeState *state) {
    GSource *source = g_timeout_source_new(milliseconds);

    g_source_set_callback(source, callback, state, NULL);
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
