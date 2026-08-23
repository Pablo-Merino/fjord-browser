#include "wpe_smoke.h"

#include <dirent.h>
#include <glib.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpe/headless/wpe-headless.h>
#include <wpe/webkit.h>

typedef struct {
    GMainContext *context;
    GMainLoop *loop;
    WebKitWebView *web_view;
    FjordWpeSmokeReport *report;
    const char *expected_title;
    char *error_message;
    gboolean termination_requested;
} SmokeState;

static void set_error(SmokeState *state, const char *message) {
    if (!state->error_message)
        state->error_message = g_strdup(message);
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
        if (report->planes)
            report->stride = wpe_buffer_dma_buf_get_stride(dma_buf, 0);
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

static void capture_display_capabilities(SmokeState *state, WPEDisplay *display) {
    FjordWpeSmokeReport *report = state->report;
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

static gboolean has_sandboxed_descendant(void) {
    char self_namespace[PATH_MAX];
    ssize_t self_length;
    DIR *proc;
    struct dirent *entry;

    self_length = readlink("/proc/self/ns/user", self_namespace, sizeof(self_namespace) - 1);
    if (self_length < 0)
        return FALSE;
    self_namespace[self_length] = '\0';

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

        g_snprintf(namespace_path, sizeof(namespace_path), "/proc/%ld/ns/user", value);
        namespace_length = readlink(namespace_path, namespace, sizeof(namespace) - 1);
        if (namespace_length < 0)
            continue;
        namespace[namespace_length] = '\0';
        if (g_strcmp0(self_namespace, namespace) != 0) {
            closedir(proc);
            return TRUE;
        }
    }

    closedir(proc);
    return FALSE;
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
    webkit_web_view_terminate_web_process(state->web_view);
}

static void load_changed(WebKitWebView *web_view, WebKitLoadEvent event, SmokeState *state) {
    (void)web_view;

    if (event == WEBKIT_LOAD_COMMITTED)
        state->report->committed = true;
    if (event == WEBKIT_LOAD_FINISHED)
        state->report->finished = true;
    maybe_terminate(state);
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

static void attach_timeout(GMainContext *context, guint milliseconds, GSourceFunc callback, SmokeState *state) {
    GSource *source = g_timeout_source_new(milliseconds);

    g_source_set_callback(source, callback, state, NULL);
    g_source_attach(source, context);
    g_source_unref(source);
}

int fjord_wpe_smoke_run(
    const char *data_directory,
    const char *cache_directory,
    const char *uri,
    FjordWpeSmokeReport *report,
    char **error_message
) {
    static const char fixture[] =
        "<!doctype html><title>loading</title><body>fjord</body>"
        "<script>document.title='Fjord WPE smoke';"
        "setTimeout(() => document.body.style.background='#111', 50);</script>";
    SmokeState state = {0};
    WPEDisplay *display = NULL;
    WebKitWebContext *web_context = NULL;
    WebKitNetworkSession *network_session = NULL;
    WPEView *view = NULL;
    WPEToplevel *toplevel = NULL;

    g_return_val_if_fail(data_directory, 1);
    g_return_val_if_fail(cache_directory, 1);
    g_return_val_if_fail(report, 1);
    g_return_val_if_fail(error_message, 1);

    memset(report, 0, sizeof(*report));
    *error_message = NULL;
    state.report = report;
    state.expected_title = uri ? NULL : "Fjord WPE smoke";
    state.context = g_main_context_new();
    g_main_context_push_thread_default(state.context);
    state.loop = g_main_loop_new(state.context, FALSE);
    report->sandbox_tools_available =
        g_find_program_in_path("bwrap") != NULL && g_find_program_in_path("xdg-dbus-proxy") != NULL;

    if (g_mkdir_with_parents(data_directory, 0700) || g_mkdir_with_parents(cache_directory, 0700)) {
        set_error(&state, "failed to create WPE profile directories");
        goto out;
    }

    display = wpe_display_headless_new();
    web_context = webkit_web_context_new();
    network_session = webkit_network_session_new(data_directory, cache_directory);
    if (!display || !web_context || !network_session) {
        set_error(&state, "failed to create WPE display, context, or network session");
        goto out;
    }
    capture_display_capabilities(&state, display);

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
    g_signal_connect(state.web_view, "notify::title", G_CALLBACK(title_changed), &state);
    g_signal_connect(state.web_view, "notify::uri", G_CALLBACK(uri_changed), &state);
    g_signal_connect(state.web_view, "web-process-terminated", G_CALLBACK(web_process_terminated), &state);
    g_signal_connect(view, "buffers-changed", G_CALLBACK(buffers_changed), &state);
    g_signal_connect(view, "buffer-rendered", G_CALLBACK(buffer_rendered), &state);
    g_signal_connect(view, "buffer-released", G_CALLBACK(buffer_released), &state);

    attach_timeout(state.context, 15000, G_SOURCE_FUNC(timeout_cb), &state);
    if (uri)
        webkit_web_view_load_uri(state.web_view, uri);
    else
        webkit_web_view_load_html(state.web_view, fixture, "fjord-smoke://fixture/");
    g_main_loop_run(state.loop);

    if (!state.error_message &&
        (!report->committed || !report->finished || !report->title_changed || !report->uri_changed ||
         !report->buffers_changed || !report->buffer_rendered || !report->buffer_released ||
         !report->web_process_terminated))
        set_error(&state, "WPE lifecycle completed without all required events");

out:
    if (state.web_view)
        g_object_unref(state.web_view);
    if (network_session)
        g_object_unref(network_session);
    if (web_context)
        g_object_unref(web_context);
    if (display)
        g_object_unref(display);
    if (state.loop)
        g_main_loop_unref(state.loop);
    g_main_context_pop_thread_default(state.context);
    g_main_context_unref(state.context);

    if (state.error_message) {
        *error_message = state.error_message;
        return 1;
    }

    return 0;
}

void fjord_wpe_smoke_free_error(char *error_message) {
    g_free(error_message);
}

size_t fjord_wpe_smoke_report_size(void) {
    return sizeof(FjordWpeSmokeReport);
}
