#include <stdint.h>
#include "dixstruct.h"
#include "windowstr.h"
#include "scrnintstr.h"
#include "xf86.h"
#include "xf86Crtc.h"

#include <mir_client_library.h>

typedef void (*xmir_buffer_available_callback)(WindowPtr win, void *ctx);
typedef struct xmir_screen xmir_screen;

typedef struct xmir_buffer_info {
    uint32_t name;
    uint32_t stride;
} xmir_buffer_info;

_X_EXPORT int
xmir_get_drm_fd(xmir_screen *screen);

_X_EXPORT xmir_screen *
xmir_screen_create(ScrnInfoPtr scrn);

_X_EXPORT Bool
xmir_mode_init(xmir_screen *screen);

_X_EXPORT void
xmir_populate_buffers_for_window(WindowPtr win, xmir_buffer_info *buf);

_X_EXPORT void
xmir_submit_rendering_for_window(WindowPtr win,
                                 RegionPtr region,
                                 xmir_buffer_available_callback callback);
