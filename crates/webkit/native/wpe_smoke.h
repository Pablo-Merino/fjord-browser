#ifndef FJORD_WPE_SMOKE_H
#define FJORD_WPE_SMOKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool committed;
    bool finished;
    bool title_changed;
    bool uri_changed;
    bool buffers_changed;
    bool buffer_rendered;
    bool buffer_released;
    bool web_process_terminated;
    bool sandbox_tools_available;
    bool sandbox_verified;
    bool explicit_sync;
    bool dma_buf_advertised;
    bool egl_imported;
    int32_t termination_reason;
    uint32_t platform_major;
    uint32_t platform_minor;
    uint32_t platform_micro;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    uint32_t planes;
    uint32_t preferred_format;
    uint32_t preferred_format_count;
    uint32_t views;
    uint32_t fd_baseline;
    uint32_t fd_after;
    uint64_t modifier;
    uint64_t preferred_modifier;
    int32_t dma_buf_fds[4];
    uint32_t offsets[4];
    uint32_t strides[4];
    char buffer_kind[16];
    char primary_node[96];
    char render_node[96];
} FjordWpeSmokeReport;

int fjord_wpe_smoke_run(
    const char *data_directory,
    const char *cache_directory,
    const char *uri,
    uint32_t views,
    FjordWpeSmokeReport *report,
    char **error_message
);

void fjord_wpe_smoke_free_error(char *error_message);

void fjord_wpe_smoke_close_fds(FjordWpeSmokeReport *report);

int fjord_wayland_subsurface_probe(void *display, void *parent_surface, char **error_message);

typedef struct FjordWpeSubsurfaceBridge FjordWpeSubsurfaceBridge;

FjordWpeSubsurfaceBridge *fjord_wpe_subsurface_bridge_new(
    void *display,
    void *parent_surface,
    char **error_message
);

int fjord_wpe_subsurface_bridge_pump(FjordWpeSubsurfaceBridge *bridge, char **error_message);

int fjord_wpe_subsurface_bridge_pointer_button(
    FjordWpeSubsurfaceBridge *bridge,
    bool pressed,
    double x,
    double y,
    char **error_message
);

int fjord_wpe_subsurface_bridge_scroll(
    FjordWpeSubsurfaceBridge *bridge,
    double x,
    double y,
    double delta_x,
    double delta_y,
    bool precise,
    char **error_message
);

int fjord_wpe_subsurface_bridge_keyboard(
    FjordWpeSubsurfaceBridge *bridge,
    bool pressed,
    uint32_t keyval,
    char **error_message
);

void fjord_wpe_subsurface_bridge_free(FjordWpeSubsurfaceBridge *bridge);

size_t fjord_wpe_smoke_report_size(void);

#endif
