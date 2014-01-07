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
#include "regionstr.h"
#include "damagestr.h"

#include "xmir.h"
#include "xmir-private.h"

#include "xf86.h"

#include <stdlib.h>
#include <unistd.h>

static DevPrivateKeyRec xmir_window_private_key;
static const RegionRec xmir_empty_region = { {0, 0, 0, 0}, &RegionBrokenData };

xmir_window *
xmir_window_get(WindowPtr win)
{
    /* The root window is handled specially */
    assert(win->parent != NULL);

    return dixGetPrivate(&win->devPrivates, &xmir_window_private_key);
}

_X_EXPORT int
xmir_window_get_fd(xmir_window *xmir_win)
{
    MirBufferPackage *package;

    if (mir_platform_type_gbm != mir_surface_get_platform_type(xmir_win->surface))
        FatalError("[xmir] Only supported on DRM Mir platform\n");

    mir_surface_get_current_buffer(xmir_win->surface, &package);
    if (package->fd_items != 1)
        FatalError("[xmir] Unexpected buffer contents from Mir; this is a programming error\n");

    return package->fd[0];
}

static void
xmir_handle_buffer_available(void *ctx)
{
    xmir_screen *xmir;
    xmir_window *mir_win = *(xmir_window **)ctx;

    if (mir_win->surface == NULL)
        return;

    xmir = xmir_screen_get(xmir_window_to_windowptr(mir_win)->drawable.pScreen);

    mir_win->has_free_buffer = TRUE;
    mir_win->damage_index = (mir_win->damage_index + 1) % MIR_MAX_BUFFER_AGE;

    if (xmir_window_is_dirty(mir_win))
        (*xmir->driver->BufferAvailableForWindow)(mir_win,
                                                  xmir_window_get_dirty(mir_win));
}

static inline int
index_in_damage_buffer(int current_index, int age)
{
    int index = (current_index - age) % MIR_MAX_BUFFER_AGE;

    return index < 0 ? MIR_MAX_BUFFER_AGE + index : index;
}

static void
handle_buffer_received(MirSurface *surf, void *ctx)
{
    xmir_window *xmir_win = ctx;

    xmir_screen *xmir =
        xmir_screen_get(xmir_window_to_windowptr(xmir_win)->drawable.pScreen);

    xmir_post_to_eventloop(xmir->submit_rendering_handler, &xmir_win);
}

static RegionPtr
damage_region_for_current_buffer(xmir_window *xmir_win)
{
    MirBufferPackage *package;
    RegionPtr region;
    int age;

    mir_surface_get_current_buffer(xmir_win->surface, &package);
    age = package->age;

    region = &xmir_win->past_damage[index_in_damage_buffer(xmir_win->damage_index, age)];

    /* As per EGL_EXT_buffer_age, contents are undefined for age == 0 */
    if (age == 0)
        RegionCopy(region, &xmir_win->region);

    return region;
}

/* Submit rendering for @window to Mir
 * @region is an (optional) damage region, to hint the compositor as to what
 * region has changed. It can be NULL to indicate the whole window should be
 * considered dirty.
 */
_X_EXPORT int
xmir_submit_rendering_for_window(xmir_window *xmir_win,
                                 RegionPtr region)
{
    RegionPtr tracking;

    if (!xmir_screen_get(xmir_win->win->drawable.pScreen)->dpms_on)
        return Success;

    xmir_win->has_free_buffer = FALSE;
    tracking = damage_region_for_current_buffer(xmir_win);
    mir_surface_swap_buffers(xmir_win->surface, &handle_buffer_received, xmir_win);

    if (region == NULL)
        RegionEmpty(tracking);
    else
        RegionSubtract(tracking, tracking, region);

    if (RegionNil(tracking))
        xorg_list_del(&xmir_win->link_damage);

    return Success;
}

_X_EXPORT Bool
xmir_window_has_free_buffer(xmir_window *xmir_win)
{
    return xmir_win->has_free_buffer;
}

_X_EXPORT RegionPtr
xmir_window_get_dirty(xmir_window *xmir_win)
{
    if (xorg_list_is_empty(&xmir_win->link_damage))
        return (RegionPtr)&xmir_empty_region;

    if (xmir_win->damaged) {
        int i;
        RegionPtr damage = DamageRegion(xmir_win->damage);
        RegionIntersect(damage, damage, &xmir_win->region);

        for (i = 0; i < MIR_MAX_BUFFER_AGE; i++) {
            RegionUnion(&xmir_win->past_damage[i],
                        &xmir_win->past_damage[i],
                        damage);
        }

        DamageEmpty(xmir_win->damage);
        xmir_win->damaged = 0;
    }

    return damage_region_for_current_buffer(xmir_win);
}

_X_EXPORT Bool
xmir_window_is_dirty(xmir_window *xmir_win)
{
    return RegionNotEmpty(xmir_window_get_dirty(xmir_win));
}

_X_EXPORT WindowPtr
xmir_window_to_windowptr(xmir_window *xmir_win)
{
    return xmir_win->win;
}

_X_EXPORT BoxPtr
xmir_window_get_drawable_region(xmir_window *xmir_win)
{
    return RegionExtents(&xmir_win->region);
}

_X_EXPORT int32_t
xmir_window_get_stride(xmir_window *xmir_win)
{
    MirBufferPackage *package;

    mir_surface_get_current_buffer(xmir_win->surface, &package);

    return package->stride;
}

static void
damage_report(DamagePtr damage, RegionPtr region, void *ctx)
{
    xmir_window *xmir_win = ctx;

    xmir_win->damaged = 1;
    xorg_list_move(&xmir_win->link_damage,
                   &xmir_screen_get(damage->pScreen)->damage_list);
}

static void
damage_destroy(DamagePtr damage, void *ctx)
{
    xmir_window *xmir_win = ctx;
    xorg_list_del(&xmir_win->link_damage);
}

void
xmir_window_enable_damage_tracking(xmir_window *xmir_win)
{
    WindowPtr win = xmir_win->win;

    if (xmir_win->damage != NULL)
        return;

    xorg_list_init(&xmir_win->link_damage);
    xmir_win->damage = DamageCreate(damage_report, damage_destroy,
                                    DamageReportNonEmpty, TRUE,
                                    win->drawable.pScreen, xmir_win);
    DamageRegister(&win->drawable, xmir_win->damage);

    for (int i = 0; i < MIR_MAX_BUFFER_AGE; i++) {
        RegionNull(&xmir_win->past_damage[i]);
    }
    xmir_win->damage_index = 0;
    xmir_win->damaged = 0;
}

void
xmir_window_disable_damage_tracking(xmir_window *xmir_win)
{
    if (xmir_win->damage != NULL) {
        DamageUnregister(&xmir_win->win->drawable, xmir_win->damage);
        DamageDestroy(xmir_win->damage);
        xmir_win->damage = NULL;
    }
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
        /* The CRTC setup has already created the root_window_fragments
           array. We need to hook the root window into it */
        for (int i = 0; xmir->root_window_fragments[i] != NULL; i++) {
            xmir->root_window_fragments[i]->win = win;

            /* TODO: This creates one Damage tracker per fragment; we only
               really need one, though */
            xmir_window_enable_damage_tracking(xmir->root_window_fragments[i]);
        }
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
        /* Break the link with the root_window_fragments */
        for (int i = 0; xmir->root_window_fragments[i] != NULL; i++) {
            xmir->root_window_fragments[i]->win = NULL;

            /* We cannot use xmir_window_disable_damage_tracking here because
             * the Damage extension will also clean it up on window destruction
            */
            xorg_list_del(&xmir->root_window_fragments[i]->link_damage);
        }
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
                              sizeof (xmir_window *));
    if (xmir->submit_rendering_handler == NULL)
        return FALSE;

    return TRUE;
}
