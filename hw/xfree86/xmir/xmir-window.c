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
#include "windowstr.h"

#include "xmir.h"
#include "xmir-private.h"

#include "xf86.h"

#include <stdlib.h>

static DevPrivateKeyRec xmir_window_private_key;

static xmir_window *
xmir_window_get(WindowPtr win)
{
    return dixLookupPrivate(&win->devPrivates, &xmir_window_private_key);
}

_X_EXPORT int
xmir_prime_fd_for_window(WindowPtr win)
{
    xmir_window *xmir_win = xmir_window_get(win);
    MirBufferPackage *package;

    assert(mir_platform_type_gbm == mir_surface_get_platform_type(xmir_win->surface));

    mir_surface_get_current_buffer(xmir_win->surface, &package);
    assert(package->fd_items == 1);

    return package->fd[0];
}

static void
xmir_handle_buffer_available(void *ctx)
{
    WindowPtr win = *(WindowPtr *)ctx;
    xmir_window *mir_win = xmir_window_get(win);
    xmir_screen *xmir = xmir_screen_get(win->drawable.pScreen);
    
    mir_win->has_free_buffer = TRUE;
    (*xmir->driver->BufferAvailableForWindow)(win);
}

static void
handle_buffer_received(MirSurface *surf, void *ctx)
{
    WindowPtr win = ctx;
    xmir_screen *xmir = xmir_screen_get(win->drawable.pScreen);

    xmir_post_to_eventloop(xmir->submit_rendering_handler, &win);
}

/* Submit rendering for @window to Mir
 * @region is an (optional) damage region, to hint the compositor as to what
 * region has changed. It can be NULL to indicate the whole window should be
 * considered dirty.
 * Once there is a new buffer available for @window, @callback will be called
 * with @context.
 * The callback is run from the main X event loop.
 */
_X_EXPORT int
xmir_submit_rendering_for_window(WindowPtr win,
                                 RegionPtr region)
{
    xmir_window *mir_win = xmir_window_get(win);

    mir_win->has_free_buffer = FALSE;
    mir_surface_swap_buffers(mir_win->surface, &handle_buffer_received, win);

    xorg_list_del(&mir_win->link_damage);
    DamageEmpty(mir_win->damage);

    return Success;
}

_X_EXPORT Bool
xmir_window_has_free_buffer(WindowPtr win)
{
    xmir_window *xmir_win = xmir_window_get(win);

    return xmir_win->has_free_buffer;
}

_X_EXPORT Bool
xmir_window_is_dirty(WindowPtr win)
{
    xmir_window *xmir_win = xmir_window_get(win);

    return RegionNotEmpty(DamageRegion(xmir_win->damage));
}

static void
damage_report(DamagePtr damage, RegionPtr region, void *ctx)
{
    xmir_window *xmir_win = ctx;
    xmir_screen *xmir = xmir_screen_get(xmir_win->win->drawable.pScreen);

    /* TODO: Is there a better way to tell if this element is in a list? */
    if (xorg_list_is_empty(&xmir_win->link_damage)) {
        xorg_list_add(&xmir_win->link_damage, &xmir->damage_list);
    }
}

static void
damage_destroy(DamagePtr damage, void *ctx)
{
    xmir_window *xmir_win = ctx;
    xorg_list_del(&xmir_win->link_damage);
}

static void
xmir_window_enable_damage_tracking(WindowPtr win)
{
    xmir_window *xmir_win = xmir_window_get(win);

    xorg_list_init(&xmir_win->link_damage);
    xmir_win->damage = DamageCreate(damage_report, damage_destroy,
                                    DamageReportNonEmpty, TRUE,
                                    win->drawable.pScreen, xmir_win);
    DamageRegister(&win->drawable, xmir_win->damage);
    DamageSetReportAfterOp(xmir_win->damage, TRUE);
}

static void
xmir_window_disable_damage_tracking(WindowPtr win)
{
    xmir_window *xmir_win = xmir_window_get(win);

    xorg_list_del(&xmir_win->link_damage);
    DamageUnregister(&win->drawable, xmir_win->damage);
    DamageDestroy(xmir_win->damage);
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
    ScreenPtr screen = win->drawable.pScreen;
    xmir_screen *xmir = xmir_screen_get(screen);
    Bool ret;

    screen->CreateWindow = xmir->CreateWindow;
    ret = (*screen->CreateWindow)(win);
    screen->CreateWindow = xmir_create_window;

    /* Until we support rootless operation, we care only for the root
     * window, which has no parent.
     */
    if (win->parent == NULL) {
        MirSurfaceParameters params;
        xmir_window *mir_win = calloc(1, sizeof *mir_win);

        if (mir_win == NULL)
            return FALSE;

        mir_win->win = win;

        params.name = "Xorg";
        params.width = win->drawable.width;
        params.height = win->drawable.height;
        /* 
         * We'll need to do something smarter here when we're rootless -
         * we'll need to distinguish between ARGB and RGB Visuals.
         */
        params.pixel_format = mir_pixel_format_xrgb_8888;
        params.buffer_usage = mir_buffer_usage_hardware;

        mir_wait_for(mir_connection_create_surface(xmir->conn,
                                                   &params,
                                                   &handle_surface_create,
                                                   mir_win));
        if (mir_win->surface == NULL) {
            free (mir_win);
            return FALSE;
        }
        dixSetPrivate(&win->devPrivates, &xmir_window_private_key, mir_win);
        /* This window now has a buffer available */
        xmir_post_to_eventloop(xmir->submit_rendering_handler, &win);
        xmir_window_enable_damage_tracking(win);
    }
    return ret;
}

static Bool
xmir_destroy_window(WindowPtr win)
{
    ScreenPtr screen = win->drawable.pScreen;
    xmir_screen *xmir = xmir_screen_get(screen);
    Bool ret;

    screen->DestroyWindow = xmir->DestroyWindow;
    ret = (*screen->DestroyWindow)(win);
    screen->DestroyWindow = xmir_destroy_window;

    /* Until we support rootless operation, we care only for the root
     * window, which has no parent.
     */
    if (win->parent == NULL) {
        xmir_window *mir_win = xmir_window_get(win);

	/* We cannot use xmir_window_disable_damage_tracking here because
	 * the Damage extension will also clean it up on window destruction
	 */
	xorg_list_del(&mir_win->link_damage);
	free(mir_win);
    }

    return ret;
}

Bool
xmir_screen_init_window(ScreenPtr screen, xmir_screen *xmir)
{
    if (!dixRegisterPrivateKey(&xmir_window_private_key, PRIVATE_WINDOW, 0))
        return FALSE;

    xmir->CreateWindow = screen->CreateWindow;
    screen->CreateWindow = xmir_create_window;
    xmir->DestroyWindow = screen->DestroyWindow;
    screen->DestroyWindow = xmir_destroy_window;

    xmir->submit_rendering_handler = 
        xmir_register_handler(&xmir_handle_buffer_available,
                              sizeof (WindowPtr));

    if (xmir->submit_rendering_handler == NULL)
        return FALSE;

    return TRUE;
}
