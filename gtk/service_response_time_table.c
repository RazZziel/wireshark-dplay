/* service_response_time_table.c
 * service_response_time_table   2003 Ronnie Sahlberg
 * Helper routines common to all service response time statistics
 * tap.
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <gtk/gtk.h>

#include "epan/packet_info.h"

#include "../simple_dialog.h"
#include "../globals.h"
#include "../color.h"

#include "gtk/service_response_time_table.h"
#include "gtk/filter_utils.h"
#include "gtk/gui_utils.h"
#include "gtk/gtkglobals.h"

#include "image/clist_ascend.xpm"
#include "image/clist_descend.xpm"


typedef struct column_arrows {
	GtkWidget *table;
	GtkWidget *ascend_pm;
	GtkWidget *descend_pm;
} column_arrows;


static void
srt_click_column_cb(GtkCList *clist, gint column, gpointer data)
{
	column_arrows *col_arrows = (column_arrows *) data;
	int i;

	gtk_clist_freeze(clist);

	for (i = 0; i < 6; i++) {
		gtk_widget_hide(col_arrows[i].ascend_pm);
		gtk_widget_hide(col_arrows[i].descend_pm);
	}

	if (column == clist->sort_column) {
		if (clist->sort_type == GTK_SORT_ASCENDING) {
			clist->sort_type = GTK_SORT_DESCENDING;
			gtk_widget_show(col_arrows[column].descend_pm);
		} else {
			clist->sort_type = GTK_SORT_ASCENDING;
			gtk_widget_show(col_arrows[column].ascend_pm);
		}
	} else {
		/* Columns 2-5   Count, Min, Max, Avg are sorted in descending
			order by default.
		   Columns 0 and 1 sort by ascending order by default
		*/
		if(column>=2){
			clist->sort_type = GTK_SORT_DESCENDING;
			gtk_widget_show(col_arrows[column].descend_pm);
		} else {
			clist->sort_type = GTK_SORT_ASCENDING;
			gtk_widget_show(col_arrows[column].ascend_pm);
		}
		gtk_clist_set_sort_column(clist, column);
	}
	gtk_clist_thaw(clist);

	gtk_clist_sort(clist);
}

static gint
srt_sort_column(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	char *text1 = NULL;
	char *text2 = NULL;
	int i1, i2;
	float f1,f2;

	const GtkCListRow *row1 = ptr1;
	const GtkCListRow *row2 = ptr2;

	text1 = GTK_CELL_TEXT (row1->cell[clist->sort_column])->text;
	text2 = GTK_CELL_TEXT (row2->cell[clist->sort_column])->text;

	switch(clist->sort_column){
	case 1:
		return strcmp (text1, text2);
	case 0:
	case 2:
		i1=atoi(text1);
		i2=atoi(text2);
		return i1-i2;
	case 3:
	case 4:
	case 5:
		sscanf(text1,"%f",&f1);
		sscanf(text2,"%f",&f2);
		if(fabs(f1-f2)<0.000005)
			return 0;
		if(f1>f2)
			return 1;
		return -1;
	}
	g_assert_not_reached();
	return 0;
}


static void
srt_select_filter_cb(GtkWidget *widget _U_, gpointer callback_data, guint callback_action)
{
	int selection;
	srt_stat_table *rst = (srt_stat_table *)callback_data;
	char *str = NULL;


	if(rst->filter_string==NULL){
		return;
	}

	selection=GPOINTER_TO_INT(g_list_nth_data(GTK_CLIST(rst->table)->selection, 0));
	if(selection>=(int)rst->num_procs){
		simple_dialog(ESD_TYPE_ERROR, ESD_BTN_OK, "No procedure selected");
		return;
	}
	/* translate it back from row index to index in procedures array */
	selection=GPOINTER_TO_INT(gtk_clist_get_row_data(rst->table, selection));

	str = g_strdup_printf("%s==%d", rst->filter_string, selection);

	apply_selected_filter (callback_action, str);

	g_free(str);
}

static gint
srt_show_popup_menu_cb(void *widg _U_, GdkEvent *event, srt_stat_table *rst)
{
	GdkEventButton *bevent = (GdkEventButton *)event;

	if(event->type==GDK_BUTTON_PRESS && bevent->button==3){
		gtk_menu_popup(GTK_MENU(rst->menu), NULL, NULL, NULL, NULL, 
			bevent->button, bevent->time);
	}

	return FALSE;
}

static GtkItemFactoryEntry srt_list_menu_items[] =
{
	/* Match */
	{"/Apply as Filter", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Apply as Filter/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Apply as Filter/... not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_NOT_SELECTED, 0), NULL, NULL,},
	{"/Apply as Filter/.. and Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_AND_SELECTED, 0), NULL, NULL,},
	{"/Apply as Filter/... or Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_OR_SELECTED, 0), NULL, NULL,},
	{"/Apply as Filter/... and not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_AND_NOT_SELECTED, 0), NULL, NULL,},
	{"/Apply as Filter/... or not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_MATCH(ACTYPE_OR_NOT_SELECTED, 0), NULL, NULL,},

	/* Prepare */
	{"/Prepare a Filter", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Prepare a Filter/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Prepare a Filter/Not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_NOT_SELECTED, 0), NULL, NULL,},
	{"/Prepare a Filter/... and Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_AND_SELECTED, 0), NULL, NULL,},
	{"/Prepare a Filter/... or Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_OR_SELECTED, 0), NULL, NULL,},
	{"/Prepare a Filter/... and not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_AND_NOT_SELECTED, 0), NULL, NULL,},
	{"/Prepare a Filter/... or not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_PREPARE(ACTYPE_OR_NOT_SELECTED, 0), NULL, NULL,},

	/* Find Frame */
	{"/Find Frame", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Find Frame/Find Frame", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Find Frame/Find Frame/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_FRAME(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Find Frame/Find Frame/Not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_FRAME(ACTYPE_NOT_SELECTED, 0), NULL, NULL,},
	/* Find Next */
	{"/Find Frame/Find Next", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Find Frame/Find Next/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_NEXT(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Find Frame/Find Next/Not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_NEXT(ACTYPE_NOT_SELECTED, 0), NULL, NULL,},

	/* Find Previous */
	{"/Find Frame/Find Previous", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Find Frame/Find Previous/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_PREVIOUS(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Find Frame/Find Previous/Not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_FIND_PREVIOUS(ACTYPE_NOT_SELECTED, 0), NULL, NULL,},

	/* Colorize Procedure */
	{"/Colorize Procedure", NULL, NULL, 0, "<Branch>", NULL,},
	{"/Colorize Procedure/Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_COLORIZE(ACTYPE_SELECTED, 0), NULL, NULL,},
	{"/Colorize Procedure/Not Selected", NULL,
		GTK_MENU_FUNC(srt_select_filter_cb), CALLBACK_COLORIZE(ACTYPE_NOT_SELECTED, 0), NULL, NULL,}

};

static void
srt_create_popup_menu(srt_stat_table *rst)
{
	GtkItemFactory *item_factory;

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<main>", NULL);

	gtk_item_factory_create_items_ac(item_factory, sizeof(srt_list_menu_items)/sizeof(srt_list_menu_items[0]), srt_list_menu_items, rst, 2);

	rst->menu = gtk_item_factory_get_widget(item_factory, "<main>");
	g_signal_connect(rst->table, "button_press_event", G_CALLBACK(srt_show_popup_menu_cb), rst);
}


void
init_srt_table(srt_stat_table *rst, int num_procs, GtkWidget *vbox, const char *filter_string)
{
	int i, j;
	column_arrows *col_arrows;
	GdkBitmap *ascend_bm, *descend_bm;
	GdkPixmap *ascend_pm, *descend_pm;
	GtkStyle *win_style;
	GtkWidget *column_lb;
	const char *default_titles[] = { "Index", "Procedure", "Calls", "Min SRT", "Max SRT", "Avg SRT" };


	if(filter_string){
		rst->filter_string=g_strdup(filter_string);
	} else {
		rst->filter_string=NULL;
	}
	rst->scrolled_window=scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), rst->scrolled_window, TRUE, TRUE, 0);

	rst->table=(GtkCList *)gtk_clist_new(6);

	gtk_widget_show(GTK_WIDGET(rst->table));
	gtk_widget_show(rst->scrolled_window);

	col_arrows = (column_arrows *) g_malloc(sizeof(column_arrows) * 6);
	win_style = gtk_widget_get_style(rst->scrolled_window);
	ascend_pm = gdk_pixmap_create_from_xpm_d(rst->scrolled_window->window,
			&ascend_bm,
			&win_style->bg[GTK_STATE_NORMAL],
			(gchar **)clist_ascend_xpm);
	descend_pm = gdk_pixmap_create_from_xpm_d(rst->scrolled_window->window,
			&descend_bm,
			&win_style->bg[GTK_STATE_NORMAL],
			(gchar **)clist_descend_xpm);
	for (i = 0; i < 6; i++) {
		col_arrows[i].table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_col_spacings(GTK_TABLE(col_arrows[i].table), 5);
		column_lb = gtk_label_new(default_titles[i]);
		gtk_table_attach(GTK_TABLE(col_arrows[i].table), column_lb, 0, 1, 0, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
		gtk_widget_show(column_lb);

		col_arrows[i].ascend_pm = gtk_pixmap_new(ascend_pm, ascend_bm);
		gtk_table_attach(GTK_TABLE(col_arrows[i].table), col_arrows[i].ascend_pm, 1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
		col_arrows[i].descend_pm = gtk_pixmap_new(descend_pm, descend_bm);
		gtk_table_attach(GTK_TABLE(col_arrows[i].table), col_arrows[i].descend_pm, 1, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
		if (i == 2) {
			gtk_widget_show(col_arrows[i].descend_pm);
		}
		gtk_clist_set_column_widget(GTK_CLIST(rst->table), i, col_arrows[i].table);
		gtk_widget_show(col_arrows[i].table);
	}
	gtk_clist_column_titles_show(GTK_CLIST(rst->table));

	gtk_clist_set_compare_func(rst->table, srt_sort_column);
	gtk_clist_set_sort_column(rst->table, 2);
	gtk_clist_set_sort_type(rst->table, GTK_SORT_DESCENDING);


	/*XXX instead of this we should probably have some code to
		dynamically adjust the width of the columns */
	gtk_clist_set_column_width(rst->table, 0, 32);
	gtk_clist_set_column_width(rst->table, 1, 160);
	gtk_clist_set_column_width(rst->table, 2, 50);
	gtk_clist_set_column_width(rst->table, 3, 60);
	gtk_clist_set_column_width(rst->table, 4, 60);
	gtk_clist_set_column_width(rst->table, 5, 60);

	gtk_clist_set_shadow_type(rst->table, GTK_SHADOW_IN);
	gtk_clist_column_titles_show(rst->table);
	gtk_container_add(GTK_CONTAINER(rst->scrolled_window), (GtkWidget *)rst->table);

	g_signal_connect(rst->table, "click-column", G_CALLBACK(srt_click_column_cb), col_arrows);

	gtk_widget_show(GTK_WIDGET(rst->table));
	gtk_widget_show(rst->scrolled_window);


	rst->num_procs=num_procs;
	rst->procedures=g_malloc(sizeof(srt_procedure_t)*num_procs);
	for(i=0;i<num_procs;i++){
		time_stat_init(&rst->procedures[i].stats);
		for(j=0;j<6;j++){
			rst->procedures[i].entries[j]=NULL;
		}
	}

	/* create popup menu for this table */
	if(rst->filter_string){
		srt_create_popup_menu(rst);
	}
}

void
init_srt_table_row(srt_stat_table *rst, int index, const char *procedure)
{
	/* we have discovered a new procedure. Extend the table accordingly */
	if(index>=rst->num_procs){
		int old_num_procs=rst->num_procs;
		int i,j;
		rst->num_procs=index+1;
		rst->procedures=g_realloc(rst->procedures, sizeof(srt_procedure_t)*(rst->num_procs));
		for(i=old_num_procs;i<rst->num_procs;i++){
			time_stat_init(&rst->procedures[i].stats);
			for(j=0;j<6;j++){
				rst->procedures[i].entries[j]=NULL;
			}
		}
	}
	rst->procedures[index].entries[0]=g_strdup_printf("%d", index);

	rst->procedures[index].entries[1]=g_strdup(procedure);

	rst->procedures[index].entries[2]=g_strdup("0");
	rst->procedures[index].entries[3]=g_strdup("0");
	rst->procedures[index].entries[4]=g_strdup("0");
	rst->procedures[index].entries[5]=g_strdup("0");
}

void
add_srt_table_data(srt_stat_table *rst, int index, const nstime_t *req_time, packet_info *pinfo)
{
	srt_procedure_t *rp;
	nstime_t t, delta;
	gint row;

	rp=&rst->procedures[index];

	/*
	 * If the count of calls for this procedure is currently zero, it's
	 * going to become non-zero, so add a row for it (we don't want
	 * rows for procedures that have no calls - especially if the
	 * procedure has no calls because the index doesn't correspond
	 * to a procedure, but is an unused/reserved value).
	 *
	 * (Yes, this means that the rows aren't in order by anything
	 * interesting.  That's why we have the table sorted by a column.)
	 */
	if (rp->stats.num==0){
		row=gtk_clist_append(rst->table, rst->procedures[index].entries);
		gtk_clist_set_row_data(rst->table, row, (gpointer)(long) index);
	}

	/* calculate time delta between request and reply */
	t=pinfo->fd->abs_ts;
	nstime_delta(&delta, &t, req_time);

	time_stat_update(&rp->stats, &delta, pinfo);
}

void
draw_srt_table_data(srt_stat_table *rst)
{
	int i,j;
	guint64 td;
	char *strp;

	for(i=0;i<rst->num_procs;i++){
		/* ignore procedures with no calls (they don't have CList rows) */
		if(rst->procedures[i].stats.num==0){
			continue;
		}

		/* scale it to units of 10us.*/
		/* for long captures with a large tot time, this can overflow on 32bit */
		td=(int)rst->procedures[i].stats.tot.secs;
		td=td*100000+(int)rst->procedures[i].stats.tot.nsecs/10000;
		td/=rst->procedures[i].stats.num;

		j=gtk_clist_find_row_from_data(rst->table, (gpointer)(long)i);
		strp=g_strdup_printf("%d", rst->procedures[i].stats.num);
		gtk_clist_set_text(rst->table, j, 2, strp);
		g_free(rst->procedures[i].entries[2]);
		rst->procedures[i].entries[2]=strp;


		strp=g_strdup_printf("%3d.%05d",
		    (int)rst->procedures[i].stats.min.secs,
		    rst->procedures[i].stats.min.nsecs/10000);
		gtk_clist_set_text(rst->table, j, 3, strp);
		g_free(rst->procedures[i].entries[3]);
		rst->procedures[i].entries[3]=strp;


		strp=g_strdup_printf("%3d.%05d",
		    (int)rst->procedures[i].stats.max.secs,
		    rst->procedures[i].stats.max.nsecs/10000);
		gtk_clist_set_text(rst->table, j, 4, strp);
		g_free(rst->procedures[i].entries[4]);
		rst->procedures[i].entries[4]=strp;

		strp=g_strdup_printf("%3" G_GINT64_MODIFIER "d.%05" G_GINT64_MODIFIER "d",
		    td/100000, td%100000);
		gtk_clist_set_text(rst->table, j, 5, strp);
		g_free(rst->procedures[i].entries[5]);
		rst->procedures[i].entries[5]=strp;
	}

	gtk_clist_sort(rst->table);
}


void
reset_srt_table_data(srt_stat_table *rst)
{
	int i;

	for(i=0;i<rst->num_procs;i++){
		time_stat_init(&rst->procedures[i].stats);
	}
	gtk_clist_clear(rst->table);
}

void
free_srt_table_data(srt_stat_table *rst)
{
	int i,j;

	for(i=0;i<rst->num_procs;i++){
		for(j=0;j<6;j++){
			if(rst->procedures[i].entries[j]){
				g_free(rst->procedures[i].entries[j]);
				rst->procedures[i].entries[j]=NULL;
			}
		}
	}
	g_free(rst->filter_string);
	rst->filter_string=NULL;
	g_free(rst->procedures);
	rst->procedures=NULL;
	rst->num_procs=0;
}

