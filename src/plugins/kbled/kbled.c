/**
 * Copyright (c) 2006 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>

#include <X11/XKBlib.h>

#include "panel.h"
#include "misc.h"
#include "plugin.h"

enum {
    CapsLock = 0,
    NumLock,
    ScrlLock
};

const char* on_icons[]={
    "capslock-on.png",
    "numlock-on.png",
    "scrllock-on.png"
};

const char* off_icons[]={
    "capslock-off.png",
    "numlock-off.png",
    "scrllock-off.png"
};

static int xkb_event_base = 0;
static int xkb_error_base = 0;

typedef struct _KbLed{
    GtkWidget *mainw;
    GtkWidget *img[3];
    GtkTooltips* tooltips;
    int old_state;
} KbLed;

static void update_display( Plugin* p, unsigned int state )
{
    int i;
    KbLed* kl = (KbLed*)p->priv;

    for( i = 0; i < 3; ++i )
    {
        gboolean old = kl->old_state & (1 << i);
        gboolean cur = state & (1 << i);
        if( old != cur )
        {
            char* file = g_build_filename( PACKAGE_DATA_DIR "/lxpanel/images",
                                                    cur ? on_icons[i] : off_icons[i], NULL );
            gtk_image_set_from_file( kl->img[ i ], file );
            g_free( file );
        }
    }
    kl->old_state = state;
}

GdkFilterReturn
event_filter(GdkXEvent *gdkxevent, GdkEvent *event, Plugin* p)
{
    XEvent* xev = (XEvent*)gdkxevent;
    if( xev->xany.type == xkb_event_base + XkbEventCode )
    {
        XkbEvent* xkbev = (XkbEvent*)xev;
        if (xkbev->any.xkb_type == XkbIndicatorStateNotify)
            update_display(p, xkbev->indicators.state);
    }
/*
    XkbEvent ev;
    memcpy(&ev.core, gdkxevent, sizeof(ev.core));

    if (ev.core.type == applet->xkbev + XkbEventCode)
    {
        if (ev.any.xkb_type == XkbIndicatorStateNotify)
            ledstates_changed(applet, ev.indicators.state);
    }
*/
    return GDK_FILTER_CONTINUE;
}

static gboolean on_button_press (GtkWidget* widget, GdkEventButton* evt, Plugin* p)
{
    KbLed *kl = (KbLed*)p->priv;
    if( evt->button == 3 ) { /* Right click*/
        GtkMenu* popup = lxpanel_get_panel_menu( p->panel, p, FALSE );
        gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
        return TRUE;
    }
    return FALSE;
}

static void kbled_orientation( Plugin* p )
{
    KbLed* kl = (KbLed*)p->priv;
    GtkWidget* newbox;
    newbox = recreate_box( (GtkBox*)kl->mainw, p->panel->orientation);
    if( newbox != kl->mainw ) {
        /* Since the old box has been destroyed,
        we need to re-add the new box to the container */
        kl->mainw = newbox;
        gtk_container_add( GTK_CONTAINER(p->pwid), kl->mainw );
    }
}

static void
kbled_destructor(Plugin *p)
{
    KbLed *kl = (KbLed*)p->priv;

    gdk_window_remove_filter(NULL, (GdkFilterFunc)event_filter, p);

    g_object_unref( kl->tooltips );
    g_free( kl );
}

/* Initialize the xkb extension */
static gboolean init_xkb()
{
    int opcode;
    int maj = XkbMajorVersion;
    int min = XkbMinorVersion;

    if (!XkbLibraryVersion(&maj, &min))
        return FALSE;
    if (!XkbQueryExtension( GDK_DISPLAY(), &opcode, &xkb_event_base, &xkb_error_base, &maj, &min))
        return FALSE;
    return TRUE;
}

static int kbled_constructor(Plugin *p, char **fp)
{
    KbLed *kl;
    GtkWidget *image;
/*
    line s;
    GdkPixbuf *icon;
    GtkIconTheme* theme;
    GtkIconInfo* info;
*/
    int i, state;

    if( ! xkb_event_base )  /* if xkb extension is not initialized */
    {
        if( G_UNLIKELY( ! init_xkb() ) )
            return FALSE;
    }
    if (!XkbSelectEvents(GDK_DISPLAY(), XkbUseCoreKbd, XkbIndicatorStateNotifyMask, XkbIndicatorStateNotifyMask))
        return FALSE;

    /* s.len = 256; */
    kl = g_new0( KbLed, 1);
    g_return_val_if_fail(kl != NULL, 0);
    p->priv = kl;

    p->pwid = gtk_event_box_new();
    gtk_widget_add_events( p->pwid, GDK_BUTTON_PRESS_MASK );
    g_signal_connect( p->pwid, "button-press-event",
            G_CALLBACK(on_button_press), p );

    kl->mainw = p->panel->my_box_new( FALSE, 0 );
    for( i =0; i < 3; ++i )
    {
        image = gtk_image_new();
        gtk_widget_set_size_request( image, 22, 22 );
        gtk_box_pack_start( (GtkBox*)kl->mainw, image, FALSE, FALSE, 0 );
        kl->img[i] = image;
    }
    gtk_container_add( (GtkContainer*)p->pwid, kl->mainw );

    XkbGetIndicatorState(GDK_DISPLAY(), XkbUseCoreKbd, &state);
    kl->old_state = ~state;
    update_display( p, state );

    /* add event filter to monitor xkb events */
    gdk_window_add_filter(NULL, (GdkFilterFunc)event_filter, p );

    gtk_widget_show_all(kl->mainw);
    kl->tooltips = g_object_ref( p->panel->tooltips );
    //gtk_tooltips_set_tip (vol->tooltips, vol->mainw, _("Volume control"), NULL);

    return TRUE;
}


PluginClass kbled_plugin_class = {
    fname: NULL,
    count: 0,

    type : "kbled",
    name : N_("Keyboard Led"),
    version: "1.0",
    description : N_("Indicators for CapsLock, NumLock, and ScrollLock keys"),

    constructor : kbled_constructor,
    destructor  : kbled_destructor,
    config : NULL,
    save : NULL,
    orientation : kbled_orientation
};