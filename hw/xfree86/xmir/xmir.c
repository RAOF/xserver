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
#include <xorg-config.h>
#endif

#include "xmir.h"

#include "xf86Module.h"
#include "xf86Priv.h"
#include "xf86.h"
#include "dixstruct.h"
#include "windowstr.h"
#include "scrnintstr.h"

#include <mir_client_library.h>

static DevPrivateKeyRec xmir_screen_private_key;
static MirConnection *conn;

_X_EXPORT int
xmir_get_drm_fd(void)
{
    MirPlatformPackage platform;

    mir_connection_get_platform(conn, &platform);
    assert(platform.fd_items == 1);
    return platform.fd[0];
}

struct submit_rendering_closure {
    xmir_screen *xmir;
    WindowPtr window;
};

static void
handle_next_buffer(MirSurface *surface, void *ctx)
{
    struct submit_rendering_closure *data = ctx;
    MirBufferPackage buf;
    XMirBufferInfo info;
    
    mir_surface_get_current_buffer(data->xmir->root_surf, &buf);
    info.name = buf.data[0];
    info.stride = buf.data[1];
    (*data->xmir->BufferNotify)(data->window, &info);
}

_X_EXPORT void
xmir_submit_rendering(xmir_screen *xmir, WindowPtr window)
{
    struct submit_rendering_closure data;
    data.xmir = xmir;
    data.window = window;
    mir_surface_next_buffer(xmir->root_surf, handle_next_buffer, xmir);
}

static xmir_screen *
xmir_get_screen(ScreenPtr screen)
{
    return dixLookupPrivate(&screen->devPrivates, &xmir_screen_private_key);
}

static void
handle_surface_create(MirSurface *surface, void *ctx)
{
    xmir_screen *screen = ctx;
    screen->root_surf = surface;
}

static Bool
xmir_realize_window(WindowPtr window)
{
    ScreenPtr screen = window->drawable.pScreen;
    xmir_screen *xmir = xmir_get_screen(screen);
    MirSurfaceParameters root_parameters;
    MirBufferPackage buf;
    XMirBufferInfo info;
    Bool ret;

    screen->RealizeWindow = xmir->RealizeWindow;
    ret = (*screen->RealizeWindow)(window);
    screen->RealizeWindow = xmir_realize_window;

    /* We only care about the root window */
    if (window->parent)
        return ret;

    root_parameters.name = "Woot!";
    root_parameters.width = window->drawable.width;
    root_parameters.height = window->drawable.height;
    root_parameters.pixel_format = mir_pixel_format_rgba_8888;

    mir_wait_for(mir_surface_create(conn, &root_parameters, handle_surface_create, xmir));
    mir_surface_get_current_buffer(xmir->root_surf, &buf);
    info.name = buf.data[0];
    info.stride = buf.data[1];
    (*xmir->BufferNotify)(window, &info);
    return ret;
}

_X_EXPORT Bool
xmir_screen_create(ScrnInfoPtr scrn)
{
    xmir_screen *xmir = NULL;
    ScreenPtr screen = xf86ScrnToScreen(scrn);

    if (!dixRegisterPrivateKey(&xmir_screen_private_key, PRIVATE_SCREEN, 0))
	return FALSE;

    xmir = calloc(sizeof (*xmir), 1);
    if (!xmir)
        return FALSE;

    dixSetPrivate(&screen->devPrivates, &xmir_screen_private_key, xmir);

    xmir->RealizeWindow = screen->RealizeWindow;
    screen->RealizeWindow = xmir_realize_window;

    /* xmir->UnrealizeWindow = screen->UnrealizeWindow; */
    /* screen->UnrealizeWindow = xmir_unrealize_window; */

    /* xmir->SetWindowPixmap = screen->SetWindowPixmap; */
    /* screen->SetWindowPixmap = xmir_set_window_pixmap; */

    xmir->BufferNotify = buffer_notify;

    return xmir;
}

static MODULESETUPPROTO(xMirSetup);
static MODULETEARDOWNPROTO(xMirTeardown);

static XF86ModuleVersionInfo VersRec = {
    "xmir",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_EXTENSION,
    ABI_EXTENSION_VERSION,
    MOD_CLASS_NONE,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData xMirModuleData = { &VersRec, xMirSetup, xMirTeardown };

static void
handle_connection(MirConnection *connection, void *ctx)
{
    (void)ctx;
    conn = connection;
}

static pointer
xMirSetup(pointer module, pointer opts, int *errmaj, int *errmin)
    static Bool setupDone = FALSE;
{
    
    if (setupDone) {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;
        return NULL;
    }

    mir_connect("/tmp/mir_socket", "OMG XSERVER", handle_connection, NULL);

    setupDone = TRUE;

    return module;
}

static void
xMirTeardown(pointer module)
{
    mir_connection_release(conn);
}
