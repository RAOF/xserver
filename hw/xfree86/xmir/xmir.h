/*
 * Copyright © 2012 Canonical, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Christopher James Halse Rogers (christopher.halse.rogers@canonical.com)
 */

#include <stdint.h>
#include <mir_client_library.h>

#include "scrnintstr.h"
#include "window.h"

typedef void (*xmir_buffer_available_callback)(WindowPtr win, void *ctx);

typedef struct xmir_screen xmir_screen;

typedef struct xmir_buffer_info {
    uint32_t name;
    uint32_t stride;
} xmir_buffer_info;

_X_EXPORT int
xmir_get_drm_fd(xmir_screen *screen);

_X_EXPORT Bool
xmir_auth_drm_magic(xmir_screen *screen, uint32_t magic);

_X_EXPORT xmir_screen *
xmir_screen_create(ScreenPtr scrn);

_X_EXPORT Bool
xmir_mode_init(ScreenPtr screen);

_X_EXPORT Bool
xmir_populate_buffers_for_window(WindowPtr win, xmir_buffer_info *buf);

_X_EXPORT void
xmir_submit_rendering_for_window(WindowPtr win,
                                 RegionPtr region,
                                 xmir_buffer_available_callback callback,
                                 void *context);
