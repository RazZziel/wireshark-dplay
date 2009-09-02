/* plugins_dlg.c
 * Dialog boxes for plugins
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include <epan/plugins.h>

#include "../globals.h"

#include "gtk/dlg_utils.h"
#include "gtk/gui_utils.h"
#include "gtk/plugins_dlg.h"


#ifdef HAVE_PLUGINS

/*
 * Fill the list widget with a list of the plugin modules.
 */
static void
plugins_scan(GtkWidget *list)
{
    plugin     *pt_plug;
    GString    *type;
    const char       *sep;

    for (pt_plug = plugin_list; pt_plug != NULL; pt_plug = pt_plug->next)
    {
        type = g_string_new("");
        sep = "";
        if (pt_plug->register_protoinfo)
        {
            type = g_string_append(type, "dissector");
            sep = ", ";
        }
        if (pt_plug->register_tap_listener)
        {
            type = g_string_append(type, sep);
            type = g_string_append(type, "tap");
            sep = ", ";
        }
        if (pt_plug->register_wtap_module)
        {
            type = g_string_append(type, sep);
            type = g_string_append(type, "file format");
            sep = ", ";
        }
        if (pt_plug->register_codec_module)
        {
            type = g_string_append(type, sep);
            type = g_string_append(type, "codec");
        }
        simple_list_append(list, 0, pt_plug->name, 1, pt_plug->version,
                           2, type->str, -1);
        g_string_free(type, TRUE);
    }
}


GtkWidget *
about_plugins_page_new(void)
{
    GtkWidget *scrolledwindow;
    GtkWidget *plugins_list;
    const gchar     *titles[] = {"Name", "Version", "Type"};

    
    scrolledwindow = scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), 
                                   GTK_SHADOW_IN);

    plugins_list = simple_list_new(3 , titles);
    plugins_scan(plugins_list);

    gtk_container_add(GTK_CONTAINER(scrolledwindow), plugins_list);

    return scrolledwindow;
}

#endif /* HAVE_PLUGINS */
