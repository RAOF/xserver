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
#include "xmir-private.h"

#include "list.h"
#include "xf86.h"
#include "xf86Priv.h"

#include <extinit.h>

#include <mir_client_library.h>
#include <mir_client_library_drm.h>

static DevPrivateKeyRec xmir_screen_private_key;

xmir_screen *
xmir_screen_get(ScreenPtr screen)
{
    return dixLookupPrivate(&screen->devPrivates, &xmir_screen_private_key);
}

_X_EXPORT int
xmir_get_drm_fd(xmir_screen *screen)
{
    MirPlatformPackage platform;

    mir_connection_get_platform(screen->conn, &platform);
    assert(platform.fd_items == 1);
    return platform.fd[0];
}

static void
handle_auth_magic(int status, void *ctx)
{
    int *retVal = ctx;
    *retVal = status;
}

_X_EXPORT int
xmir_auth_drm_magic(xmir_screen *xmir, uint32_t magic)
{
    int status;
    mir_wait_for(mir_connection_drm_auth_magic(xmir->conn,
                                               magic,
                                               &handle_auth_magic,
                                               &status));
    return status;
}

static void
handle_connection(MirConnection *connection, void *ctx)
{
    xmir_screen *xmir = ctx;
    xmir->conn = connection;
}

_X_EXPORT xmir_screen *
xmir_screen_create(ScrnInfoPtr scrn)
{
    xmir_screen *xmir = calloc (1, sizeof *xmir);
    if (xmir == NULL)
        return NULL;

    mir_wait_for(mir_connect("/tmp/mir_socket",
                             mirID,
                             handle_connection, xmir));

    if (!mir_connection_is_valid(xmir->conn)) {
        xf86Msg(X_ERROR,
                "Failed to connect to Mir: %s\n",
                mir_connection_get_error_message(xmir->conn));
        goto error;
    }

    return xmir;
error:
    if (xmir)
        free(xmir);
    return NULL;
}

_X_EXPORT Bool
xmir_screen_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir, xmir_driver *driver)
{
    xmir->driver = driver;
    xorg_list_init(&xmir->damage_list);

    noScreenSaverExtension = TRUE;

    if (!xmir_mode_pre_init(scrn, xmir))
        return FALSE;

    return TRUE;
}

_X_EXPORT Bool
xmir_screen_init(ScreenPtr screen, xmir_screen *xmir)
{
    if (!dixRegisterPrivateKey(&xmir_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;
    dixSetPrivate(&screen->devPrivates, &xmir_screen_private_key, xmir);

    if (!xmir_screen_init_window(screen, xmir))
        return FALSE;

    return TRUE;
}

_X_EXPORT void
xmir_screen_close(ScreenPtr screen, xmir_screen *xmir)
{

}

_X_EXPORT void
xmir_screen_destroy(xmir_screen *xmir)
{
    
}

_X_EXPORT void
xmir_screen_for_each_damaged_window(xmir_screen *xmir, xmir_handle_window_damage_proc callback)
{
    xmir_window *xmir_win, *tmp_win;
    xorg_list_for_each_entry_safe(xmir_win, tmp_win, &xmir->damage_list, link_damage) {
        (*callback)(xmir_win->win, xmir_win->damage);
        if(!xmir_window_is_dirty(xmir_win->win))
            xorg_list_del(&xmir_win->link_damage);
    }
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

_X_EXPORT XF86ModuleData xmirModuleData = { &VersRec, xMirSetup, xMirTeardown };

static pointer
xMirSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;
    
    if (setupDone) {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;
        return NULL;
    }

    xmir_init_thread_to_eventloop();    

    setupDone = TRUE;

    return module;
}

static void
xMirTeardown(pointer module)
{
}
