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

#ifdef HAVE_XORG_CONFIG_H
#include "xorg-config.h"
#endif
#include <xorg-server.h>

#include "xmir-private.h"

#include <stdlib.h>
#include <mir_client_library.h>

static DevPrivateKeyRec xmir_window_private_key;

typedef struct {
    MirSurface surface;
} xmir_window;

static xmir_window *
xmir_window_get(WindowPtr win)
{
    return dixLookupPrivate(&win->devPrivates, &xmir_window_private_key);
}

_X_EXPORT Bool
xmir_populate_buffers_for_window(WindowPtr win, xmir_buffer_info *bufs)
{
    xmir_window *xmir_win = xmir_get_window(win);
    MirBufferPackage package;

    mir_surface_get_current_buffer(xmir_win->surf, &package);
    assert(package->num_data == 1);
        
    bufs->name = package.data[0];
    bufs->stride = package.stride;
    return TRUE;
}

struct xmir_window_callback_closure {
    xmir_buffer_available_callback callback;
    WindowPtr win;
    void *ctx;
};

static void
handle_buffer_received(MirSurface *surf, void *ctx)
{
    struct xmir_window_callback_closure *closure = ctx;

    (*closure->callback)(closure->win, closure->ctx);

    free(closure);
}

_X_EXPORT void
xmir_submit_rendering_for_window(WindowPtr win,
                                 RegionPtr region,
                                 xmir_buffer_available_callback callback,
                                 void *context)
{
    xmir_window *mir_win = xmir_window_get(win);
    struct xmir_window_callback_closure *closure = malloc(sizeof *closure);

    /* TODO: handle failing malloc() */
    closure->win = win;
    closure->ctx = context;

    mir_surface_next_buffer(mir_win->surface, &handle_buffer_received, closure);
}

static void
handle_surface_create(MirSurface *surface, void *ctx)
{
    xmir_window *mir_win = ctx;
    mir_win->surface = surface;
}

static Bool
xmir_create_window(WindowPtr win)
{
    ScreenPtr screen = window->drawable.pScreen;
    xmir_screen *xmir = xmir_screen_get(screen);
    Bool ret;

    screen->CreateWindow = xmir->CreateWindow;
    ret = (*screen->CreateWindow)(win);
    screen->CreateWindow = xmir_create_window;

    /* Until we support rootless operation, we care only for the root
     * window, which has no parent.
     */
    if (window->parent == NULL) {
        MirSurfaceParameters params;
        xmir_window *mir_win = malloc(sizeof *mir_win);

        if (mir_win == NULL)
            return FALSE;

        params.name = "Xorg";
        params.width = win->drawable.width;
        params.height = win->drawable.height;
        /* 
         * We'll need to do something smarter here when we're rootless -
         * we'll need to distinguish between ARGB and RGB Visuals.
         */
        params.pixel_format = mir_pixel_format_rgbx_8888;
        params.buffer_usage = mir_buffer_usage_hardware;

        mir_wait_for(mir_surface_create(conn,
                                        &params,
                                        &handle_surface_create,
                                        mir_win));
        if (mir_win->surface == NULL) {
            free (mir_win);
            return FALSE;
        }
    }

    return ret;
}

Bool
xmir_screen_init_window(xmir_screen *mir_screen, ScreenPtr screen)
{
    if (!dixRegisterPrivateKey(&xmir_window_private_key, PRIVATE_WINDOW, 0))
        return FALSE;

    mir_screen->CreateWindow = pScreen->CreateWindow;

    return TRUE;
}
