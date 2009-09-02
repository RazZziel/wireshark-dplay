/* sctp_chunk_stat.c
 * SCTP chunk counter for Wireshark
 * Copyright 2005 Oleg Terletsky <oleg.terletsky [AT] comverse.com>
 * Copyright 2009 Varun Notibala <nbvarun [AT] gmail.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
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

#include <epan/packet_info.h>
#include <epan/epan.h>
#include <epan/value_string.h>
#include <epan/tap.h>
#include <epan/dissectors/packet-sctp.h>

#include "../register.h"
#include "../simple_dialog.h"
#include "../file.h"
#include "../globals.h"
#include "../stat_menu.h"

#include "gtk/gui_stat_util.h"
#include "gtk/dlg_utils.h"
#include "gtk/tap_dfilter_dlg.h"
#include "gtk/gui_utils.h"
#include "gtk/main.h"


static void sctpstat_init(const char *optarg, void *userdata);

static tap_dfilter_dlg sctp_stat_dlg = {
	"SCTP Statistics",
	"sctp,stat",
	sctpstat_init,
	-1
};

typedef struct sctp_ep {
	struct sctp_ep* next;
	address src;
	address dst;
	guint16 sport;
	guint16 dport;
	guint32 chunk_count[256];
} sctp_ep_t;

/* used to keep track of the statistics for an entire program interface */
typedef struct _sctp_stat_t {
	GtkWidget  *win;
	GtkWidget  *vbox;
	char       *filter;
	GtkWidget  *scrolled_window;
	GtkCList   *table;
	guint32    number_of_packets;
	sctp_ep_t* ep_list;
} sctpstat_t;

typedef struct _sctp_info sctp_into_t;

#define SCTP_DATA_CHUNK_ID		 0
#define SCTP_INIT_CHUNK_ID		 1
#define SCTP_INIT_ACK_CHUNK_ID		 2
#define SCTP_SACK_CHUNK_ID		 3
#define SCTP_HEARTBEAT_CHUNK_ID		 4
#define SCTP_HEARTBEAT_ACK_CHUNK_ID	 5
#define SCTP_ABORT_CHUNK_ID		 6
#define SCTP_SHUTDOWN_CHUNK_ID		 7
#define SCTP_SHUTDOWN_ACK_CHUNK_ID	 8
#define SCTP_ERROR_CHUNK_ID		 9
#define SCTP_COOKIE_ECHO_CHUNK_ID	10
#define SCTP_COOKIE_ACK_CHUNK_ID	11
#define SCTP_ECNE_CHUNK_ID		12
#define SCTP_CWR_CHUNK_ID		13
#define SCTP_SHUTDOWN_COMPLETE_CHUNK_ID 14
#define SCTP_AUTH_CHUNK_ID              15
#define SCTP_NR_SACK_CHUNK_ID	        16
#define SCTP_ASCONF_ACK_CHUNK_ID      0x80
#define SCTP_PKTDROP_CHUNK_ID	      0x81
#define SCTP_FORWARD_TSN_CHUNK_ID     0xC0
#define SCTP_ASCONF_CHUNK_ID	      0xC1
#define SCTP_IETF_EXT		      0xFF

#define CHUNK_TYPE_OFFSET 0
#define CHUNK_TYPE(x)(tvb_get_guint8((x), CHUNK_TYPE_OFFSET))

static void
sctpstat_reset(void *phs)
{
	sctpstat_t* sctp_stat = (sctpstat_t *)phs;
	sctp_ep_t* list = (sctp_ep_t*)sctp_stat->ep_list;
	sctp_ep_t* tmp = NULL;
	guint16 chunk_type;

	if(!list)
		return;

	for(tmp = list; tmp ; tmp=tmp->next)
		for(chunk_type = 0; chunk_type < 256; chunk_type++)
			tmp->chunk_count[chunk_type] = 0;

	sctp_stat->number_of_packets = 0;
}

static sctp_ep_t* alloc_sctp_ep(struct _sctp_info *si)
{
	sctp_ep_t* ep;
	guint16 chunk_type;

	if(!si)
		return NULL;

	if (!(ep = g_malloc(sizeof(sctp_ep_t))))
		return NULL;

	COPY_ADDRESS(&ep->src,&si->ip_src);
	COPY_ADDRESS(&ep->dst,&si->ip_dst);
	ep->sport = si->sport;
	ep->dport = si->dport;
	ep->next = NULL;
	for(chunk_type = 0; chunk_type < 256; chunk_type++)
		ep->chunk_count[chunk_type] = 0;
	return ep;
}

static int
sctpstat_packet(void *phs, packet_info *pinfo _U_, epan_dissect_t *edt _U_, const void *phi)
{

	sctpstat_t *hs=(sctpstat_t *)phs;
	sctp_ep_t *tmp = NULL, *te = NULL;
	struct _sctp_info *si = (struct _sctp_info *) phi;
	guint32 tvb_number;
	guint8 chunk_type;

	if (!hs)
		return (0);

	hs->number_of_packets++;
	if(!hs->ep_list) {
		hs->ep_list = alloc_sctp_ep(si);
		te = hs->ep_list;
	} else {
		for(tmp=hs->ep_list ; tmp ; tmp=tmp->next) {
			if((!CMP_ADDRESS(&tmp->src,&si->ip_src)) &&
			   (!CMP_ADDRESS(&tmp->dst,&si->ip_dst)) &&
			   (tmp->sport == si->sport) &&
			   (tmp->dport == si->dport)) {
				te = tmp;
				break;
			}
		}
		if(!te) {
			if ((te = alloc_sctp_ep(si))) {
				te->next = hs->ep_list;
				hs->ep_list = te;
			}
		}
	}

	if(!te)
		return (0);


	if (si->number_of_tvbs > 0) {
		chunk_type = CHUNK_TYPE(si->tvb[0]);
		if ((chunk_type == SCTP_INIT_CHUNK_ID) ||
		    (chunk_type == SCTP_INIT_ACK_CHUNK_ID)) {
			(te->chunk_count[chunk_type])++;
		} else {
			for(tvb_number = 0; tvb_number < si->number_of_tvbs; tvb_number++) {
				(te->chunk_count[CHUNK_TYPE(si->tvb[tvb_number])])++;
			}
		}
	}
	return (1);
}


static void
sctpstat_draw(void *phs)
{
	sctpstat_t *hs=(sctpstat_t *)phs;
	sctp_ep_t* list = hs->ep_list, *tmp=0;
	char *str[15];
	int i=0;

	for(i=0;i<15;i++) {
		str[i]=g_malloc(sizeof(char[256]));
	}
	/* Now print Message and Reason Counter Table */
	/* clear list before printing */
	gtk_clist_clear(hs->table);


	for(tmp = list ; tmp ; tmp=tmp->next) {

		g_snprintf(str[0],  sizeof(char[256]),"%s", address_to_str(&tmp->src));
		g_snprintf(str[1],  sizeof(char[256]),"%u", tmp->sport);
		g_snprintf(str[2],  sizeof(char[256]),"%s", address_to_str(&tmp->dst));
		g_snprintf(str[3],  sizeof(char[256]),"%u", tmp->dport);
		g_snprintf(str[4],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_DATA_CHUNK_ID]);
		g_snprintf(str[5],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_SACK_CHUNK_ID]);
		g_snprintf(str[6],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_HEARTBEAT_CHUNK_ID]);
		g_snprintf(str[7],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_HEARTBEAT_ACK_CHUNK_ID]);
		g_snprintf(str[8],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_INIT_CHUNK_ID]);
		g_snprintf(str[9],  sizeof(char[256]),"%u", tmp->chunk_count[SCTP_INIT_ACK_CHUNK_ID]);
		g_snprintf(str[10], sizeof(char[256]),"%u", tmp->chunk_count[SCTP_COOKIE_ECHO_CHUNK_ID]);
		g_snprintf(str[11], sizeof(char[256]),"%u", tmp->chunk_count[SCTP_COOKIE_ACK_CHUNK_ID]);
		g_snprintf(str[12], sizeof(char[256]),"%u", tmp->chunk_count[SCTP_ABORT_CHUNK_ID]);
		g_snprintf(str[13], sizeof(char[256]),"%u", tmp->chunk_count[SCTP_ERROR_CHUNK_ID]);
		g_snprintf(str[14], sizeof(char[256]),"%u", tmp->chunk_count[SCTP_NR_SACK_CHUNK_ID]);

		gtk_clist_append(hs->table, str);
	}

	gtk_widget_show(GTK_WIDGET(hs->table));

}

static void
win_destroy_cb(GtkWindow *win _U_, gpointer data)
{
	sctpstat_t *hs=(sctpstat_t *)data;

	protect_thread_critical_region();
	remove_tap_listener(hs);
	unprotect_thread_critical_region();

	if(hs->filter){
		g_free(hs->filter);
		hs->filter=NULL;
	}
	g_free(hs);
}


static const gchar *titles[]={
			"Source IP",
			"Source Port",
			"Dest IP",
			"Dest Port",
			"DATA",
			"SACK",
			"HBEAT",
			"HBEAT_ACK",
			"INIT",
			"INIT_ACK",
			"COOKIE",
			"COOKIE_ACK",
			"ABORT",
			"ERROR",
			"NR_SACK" };

static void
sctpstat_init(const char *optarg, void *userdata _U_)
{
	sctpstat_t *hs;
	GString *error_string;
	GtkWidget *bbox;
	GtkWidget *close_bt;

	hs=g_malloc(sizeof(sctpstat_t));
	if(strncmp(optarg,"sctp,stat,",10) == 0){
		hs->filter=g_strdup(optarg+10);
	} else {
		hs->filter=NULL;
	}
	hs->ep_list = NULL;
	hs->number_of_packets = 0;
	sctpstat_reset(hs);

	hs->win = dlg_window_new("Wireshark: SCTP Chunk Statistics");  /* transient_for top_level */
	gtk_window_set_destroy_with_parent (GTK_WINDOW(hs->win), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(hs->win), 600, 200);

	hs->vbox=gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hs->vbox), 12);

	init_main_stat_window(hs->win, hs->vbox, "SCTP Chunk Counter", hs->filter);

	/* init a scrolled window*/
	hs->scrolled_window = scrolled_window_new(NULL, NULL);

	hs->table = create_stat_table(hs->scrolled_window, hs->vbox, 15, titles);

	error_string=register_tap_listener("sctp", hs, hs->filter, 0,
	                                   sctpstat_reset,
	                                   sctpstat_packet,
	                                   sctpstat_draw);
	if(error_string){
		simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK, "%s", error_string->str);
		g_string_free(error_string, TRUE);
		g_free(hs->filter);
		g_free(hs);
		return;
	}

	/* Button row. */
	bbox = dlg_button_row_new(GTK_STOCK_CLOSE, NULL);
	gtk_box_pack_end(GTK_BOX(hs->vbox), bbox, FALSE, FALSE, 0);

	close_bt = g_object_get_data(G_OBJECT(bbox), GTK_STOCK_CLOSE);
	window_set_cancel_button(hs->win, close_bt, window_cancel_button_cb);

	g_signal_connect(hs->win, "delete_event", G_CALLBACK(window_delete_event_cb), NULL);
	g_signal_connect(hs->win, "destroy", G_CALLBACK(win_destroy_cb), hs);

	gtk_widget_show_all(hs->win);
	window_present(hs->win);

	cf_retap_packets(&cfile);
}

void
register_tap_listener_sctpstat(void)
{
	register_dfilter_stat(&sctp_stat_dlg, "S_CTP/Chunk Counter",
	    REGISTER_STAT_GROUP_TELEPHONY);
}
