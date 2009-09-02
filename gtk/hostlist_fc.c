/* hostlist_fc.c   2004 Ian Schorr
 * modified from endpoint_talkers_fc.c   2003 Ronnie Sahlberg
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
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
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <string.h>

#include <gtk/gtk.h>

#include "epan/packet.h"
#include <epan/stat_cmd_args.h>
#include <epan/tap.h>
#include <epan/conversation.h>
#include <epan/dissectors/packet-scsi.h>
#include <epan/dissectors/packet-fc.h>

#include "../stat_menu.h"
#include "../register.h"

#include "gtk/gui_stat_menu.h"
#include "gtk/hostlist_table.h"


static int
fc_hostlist_packet(void *pit, packet_info *pinfo, epan_dissect_t *edt _U_, const void *vip)
{
	hostlist_table *hosts=(hostlist_table *)pit;
	const fc_hdr *fchdr=vip;

	/* Take two "add" passes per packet, adding for each direction, ensures that all
	packets are counted properly (even if address is sending to itself) 
	XXX - this could probably be done more efficiently inside hostlist_table */
	add_hostlist_table_data(hosts, &fchdr->s_id, 0, TRUE, 1, pinfo->fd->pkt_len, SAT_NONE, PT_NONE);
	add_hostlist_table_data(hosts, &fchdr->d_id, 0, FALSE, 1, pinfo->fd->pkt_len, SAT_NONE, PT_NONE);

	return 1;
}



static void
gtk_fc_hostlist_init(const char *optarg, void* userdata _U_)
{
	const char *filter=NULL;

	if(!strncmp(optarg,"hosts,fc,",9)){
		filter=optarg+9;
	} else {
		filter=NULL;
	}

	init_hostlist_table(TRUE, "Fibre Channel", "fc", filter, fc_hostlist_packet);

}


static void
gtk_fc_hostlist_cb(GtkWidget *w _U_, gpointer d _U_)
{
	gtk_fc_hostlist_init("hosts,fc",NULL);
}


void
register_tap_listener_fc_hostlist(void)
{
	register_stat_cmd_arg("hosts,fc", gtk_fc_hostlist_init,NULL);

	register_stat_menu_item("Fibre Channel", REGISTER_STAT_GROUP_ENDPOINT_LIST,
	    gtk_fc_hostlist_cb, NULL, NULL, NULL);

	register_hostlist_table(TRUE, "Fibre Channel", "fc", NULL /*filter*/, fc_hostlist_packet);
}
