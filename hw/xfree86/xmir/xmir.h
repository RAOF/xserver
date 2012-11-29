#include <stdint.h>
#include "dixstruct.h"
#include "windowstr.h"
#include "scrnintstr.h"
#include "xf86.h"
#include "xf86Crtc.h"

#include <mir_client_library.h>

_X_EXPORT int
xmir_get_drm_fd(void);

_X_EXPORT Bool
xmir_mode_init(ScrnInfoPtr scrn);

_X_EXPORT Bool
xmir_start_buffer_loop(mir_surface_lifecycle_callback callback, void *ctx);

_X_EXPORT Bool
xmir_populate_buffers_for_window(WindowPtr win, MirBufferPackage *bufs);
