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

#ifndef _XMIR_H
#define _XMIR_H

#include <stdint.h>
#include <mir_toolkit/mir_client_library.h>

#include "xf86str.h"
#include "scrnintstr.h"
#include "window.h"

typedef struct xmir_screen xmir_screen;
typedef struct xmir_window xmir_window;

typedef void (*xmir_window_proc)(xmir_window *xmir_win, RegionPtr damaged_region);

#define XMIR_DRIVER_VERSION 1
typedef struct {
    int version;
    xmir_window_proc BufferAvailableForWindow;
} xmir_driver;

_X_EXPORT int
xmir_get_drm_fd(const char *busid);

_X_EXPORT int
xmir_auth_drm_magic(xmir_screen *xmir, uint32_t magic);

_X_EXPORT xmir_screen *
xmir_screen_create(ScrnInfoPtr scrn);

_X_EXPORT Bool
xmir_screen_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir, xmir_driver *driver);

_X_EXPORT Bool
xmir_screen_init(ScreenPtr screen, xmir_screen *xmir);

_X_EXPORT void
xmir_screen_close(ScreenPtr screen, xmir_screen *xmir);
	
_X_EXPORT void
xmir_screen_destroy(xmir_screen *xmir);

_X_EXPORT WindowPtr
xmir_window_to_windowptr(xmir_window *xmir_win);

_X_EXPORT int
xmir_window_get_fd(xmir_window *xmir_win);

_X_EXPORT int
xmir_submit_rendering_for_window(xmir_window *xmir_win,
                                 RegionPtr region);

_X_EXPORT Bool
xmir_window_has_free_buffer(xmir_window *xmir_win);

_X_EXPORT RegionPtr
xmir_window_get_dirty(xmir_window *xmir_win);

_X_EXPORT Bool
xmir_window_is_dirty(xmir_window *xmir_win);

_X_EXPORT BoxPtr
xmir_window_get_drawable_region(xmir_window *xmir_win);

_X_EXPORT int32_t
xmir_window_get_stride(xmir_window *xmir_win);

_X_EXPORT void
xmir_screen_for_each_damaged_window(xmir_screen *xmir, xmir_window_proc callback);

#endif /* _XMIR_H */
