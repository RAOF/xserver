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

static void
crtc_dpms(xf86CrtcPtr drmmode_crtc, int mode)
{
}

static Bool
crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
		    Rotation rotation, int x, int y)
{
	return TRUE;
}

static void
crtc_set_cursor_colors (xf86CrtcPtr crtc, int bg, int fg)
{
}

static void
crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
}

static void
crtc_show_cursor (xf86CrtcPtr crtc)
{
}

static void
crtc_hide_cursor (xf86CrtcPtr crtc)
{
}

static void
crtc_load_cursor_argb (xf86CrtcPtr crtc, CARD32 *image)
{
}

static PixmapPtr
crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	return NULL;
}

static void *
crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	return NULL;
}

static void
crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
}

static const xf86CrtcFuncsRec crtc_funcs = {
    .dpms                = crtc_dpms,
    .set_mode_major      = crtc_set_mode_major,
    .set_cursor_colors   = crtc_set_cursor_colors,
    .set_cursor_position = crtc_set_cursor_position,
    .show_cursor         = crtc_show_cursor,
    .hide_cursor         = crtc_hide_cursor,
    .load_cursor_argb    = crtc_load_cursor_argb,
    .shadow_create       = crtc_shadow_create,
    .shadow_allocate     = crtc_shadow_allocate,
    .shadow_destroy      = crtc_shadow_destroy,
    .destroy             = NULL, /* XXX */
};

static void
output_dpms(xf86OutputPtr output, int mode)
{
	return;
}

static xf86OutputStatus
output_detect(xf86OutputPtr output)
{
	return XF86OutputStatusConnected;
}

static Bool
output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	return MODE_OK;
}

struct mir_output {
    int width;
    int height;
    xf86Monitor monitor;
};

static DisplayModePtr
output_get_modes(xf86OutputPtr xf86output)
{
    struct mir_output *output = xf86output->driver_private;
    struct monitor_ranges *ranges;
    DisplayModePtr modes;

    modes = xf86CVTMode(output->width, output->height, 60, TRUE, FALSE);
    output->monitor.det_mon[0].type = DS_RANGES;
    ranges = &output->monitor.det_mon[0].section.ranges;
    ranges->min_h = modes->HSync - 10;
    ranges->max_h = modes->HSync + 10;
    ranges->min_v = modes->VRefresh - 10;
    ranges->max_v = modes->VRefresh + 10;
    ranges->max_clock = modes->Clock + 100;
    output->monitor.det_mon[1].type = DT;
    output->monitor.det_mon[2].type = DT;
    output->monitor.det_mon[3].type = DT;
    output->monitor.no_sections = 0;

    xf86output->MonInfo = &output->monitor;

    return modes;
}

static void
output_destroy(xf86OutputPtr xf86output)
{
    struct mir_output *output = xf86output->driver_private;
    
    free(output);
}

static const xf86OutputFuncsRec output_funcs = {
    .dpms	    = output_dpms,
    .detect	    = output_detect,
    .mode_valid	= output_mode_valid,
    .get_modes	= output_get_modes,
    .destroy	= output_destroy
};

static Bool
resize(ScrnInfoPtr scrn, int width, int height)
{
    if (scrn->virtualX == width && scrn->virtualY == height)
        return TRUE;
    /* We don't handle resize at all, we must match the compositor size */
    return FALSE;
}

static const xf86CrtcConfigFuncsRec config_funcs = {
    resize
};

static DevPrivateKeyRec xmir_screen_private_key;
static DevPrivateKeyRec xmir_window_private_key;

typedef struct {
    MirConnection conn;
    CreateWindowProcPtr CreateWindow;
    ResizeWindowProcPtr ResizeWindow;
} xmir_screen;

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

_X_EXPORT Bool
xmir_auth_drm_magic(xmir_screen *screen, uint32_t magic)
{
    /* Needs Mir protocol to do the auth proxy */
    return TRUE;
}

typedef struct {
    MirSurface surface;
} xmir_window;

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

static void
handle_connection(MirConnection *connection, void *ctx)
{
    xmir_screen screen = ctx;
    screen->conn = connection;
}

_X_EXPORT xmir_screen *
xmir_screen_create(ScreenPtr pScreen)
{
    xmir_screen screen = calloc (1, sizeof *screen);
    if (screen == NULL)
        return NULL;

    mir_wait_for(mir_connect("/tmp/mir_socket",
                             "OMG XSERVER",
                             handle_connection, screen);

    if (!mir_connection_is_valid(screen->conn)) {
        xf86Msg(X_ERROR,
                "Failed to connect to Mir: %s\n",
                mir_connection_get_error_message(screen->conn));
        goto error;
    }

    if (!dixRegisterPrivateKey(&xmir_screen_private_key, PRIVATE_SCREEN, 0))
        goto error;

    if (!dixRegisterPrivateKey(&xmir_window_private_key, PRIVATE_WINDOW, 0))
        goto error;

    dixSetPrivate(&pScreen->devPrivates, &xmir_screen_private_key, screen);

    screen->CreateWindow = pScreen->CreateWindow;
/*    screen->ResizeWindow = pScreen->ResizeWindow; */

    return screen;
error:
    if (screen)
        free(screen);
    return NULL;
}

xmir_window *
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

_X_EXPORT Bool
xmir_mode_init(ScrnInfoPtr scrn)
{
    MirDisplayInfo display_info;

    xf86OutputPtr xf86output;
    struct mir_output *output;
    
    mir_connection_get_display_info(conn, &display_info);
    
    
    
    /* Set up CRTC config functions */
    xf86CrtcConfigInit(scrn, &config_funcs);

    /* We don't support resizing whatsoever */
    xf86CrtcSetSizeRange(scrn,
                         display_info.width, display_info.height,
                         display_info.width, display_info.height);

    output = calloc(sizeof *output, 1);
    output->width = display_info.width;
    output->height = display_info.height;
    
    xf86output = xf86OutputCreate(scrn, &output_funcs, "XMIR-1");
    xf86output->possible_crtcs = 1;
    xf86output->possible_clones = 1;
    xf86output->driver_private = output;

    xf86CrtcCreate(scrn, &crtc_funcs);

    xf86InitialConfiguration(scrn, TRUE);
  
    return TRUE;
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

    setupDone = TRUE;

    return module;
}

static void
xMirTeardown(pointer module)
{
}
