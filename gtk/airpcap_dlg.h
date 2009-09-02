/* airpcap_dlg.h
 * Declarations of routines for the "Airpcap" dialog
 *
 * $Id$
 *
 * Giorgio Tino <giorgio.tino@cacetech.com>
 * Copyright (c) CACE Technologies, LLC 2006
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

#ifndef __AIRPCAP_DLG_H__
#define __AIRPCAP_DLG_H__

#define AIRPCAP_ADVANCED_FROM_TOOLBAR 0
#define AIRPCAP_ADVANCED_FROM_OPTIONS 1

/*
 * Returns FALSE if a text string has length 0, i.e. the first char
 * is '\0', TRUE otherwise
 */
gboolean
string_is_not_empty(gchar *s);

/*
 * Edit key window destroy callback
 */
void
on_edit_key_w_destroy(GtkWidget *button, gpointer data _U_);

/*
 * Add key window destroy callback
 */
void
on_add_key_w_destroy(GtkWidget *button, gpointer data _U_);

/*
 * Creates the list of available decryption modes, depending on the adapters found
 */
void
update_decryption_mode_list(GtkWidget *w);

/*
 * Callback for the 'Add Key' button.
 */
void
on_add_new_key_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the 'Remove Key' button.
 */
void
on_remove_key_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the 'Edit Key' button.
 */
void
on_edit_key_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the 'Move Key Down' button.
 */
void
on_move_key_down_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the 'Move Key Up' button.
 */
void
on_move_key_up_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the Wireless Advanced Settings 'Apply' button.
 */
void
on_advanced_apply_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the OK button 'clicked' in the Advanced Wireless Settings window.
 */
void
on_advanced_ok_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the CANCEL button 'clicked' in the Advanced Wireless Settings window.
 */
void
on_advanced_cancel_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the 'Apply' button.
 */
void
on_key_management_apply_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the OK button 'clicked' in the Decryption Key Management window.
 */
void
on_key_management_ok_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Callback for the CANCEL button 'clicked' in the Decryption Key Management window.
 */
void
on_key_management_cancel_bt_clicked(GtkWidget *button, gpointer data _U_);

/* the window was closed, cleanup things */
void
on_key_management_destroy(GtkWidget *w _U_, gpointer data _U_);

/*
 * Callback for the 'Reset Configuration' button.
 */
void
on_reset_configuration_bt_clicked(GtkWidget *button, gpointer data _U_);

/*
 * Turns the decryption on or off
 */
void
on_decryption_mode_cb_changed(GtkWidget *w, gpointer data);

/*
 * Selects the current decryption mode in the given combo box
 */
void
update_decryption_mode(GtkWidget *w);

/*
 * Callback for the select row event in the key list widget
 */
void
on_key_ls_select_row(GtkWidget *widget,
                     gint row,
                     gint column,
                     GdkEventButton *event,
                     gpointer data);

/*
 * Callback for the unselect row event in the key list widget
 */
void
on_key_ls_unselect_row(GtkWidget *widget,
                     gint row,
                     gint column,
                     GdkEventButton *event,
                     gpointer data);

/*
 * Callback for the click column event in the key list widget
 */
void
on_key_ls_click_column(GtkWidget *widget,
                       gint column,
                       gpointer data);

/*
 * Thread function used to blink the led
 */
gboolean update_blink(gpointer data _U_);

/*
 * Blink button callback
 */
void
on_blink_bt_clicked(GtkWidget *blink_bt _U_, gpointer if_data);

/*
 * Callback for the 'Any' adapter What's This button.
 */
void
on_what_s_this_bt_clicked( GtkWidget *blink_bt _U_, gpointer if_data );

/** Create a "Airpcap" dialog box caused by a button click.
 *
 * @param widget parent widget
 * @param construct_args_ptr parameters to construct the dialog (construct_args_t)
 */
void display_airpcap_advanced_cb(GtkWidget *widget, gpointer construct_args_ptr);

/* Called to create the key management window */
void
display_airpcap_key_management_cb(GtkWidget *w, gpointer data);

/**/
/*
 * Dialog box that appears whenever keys are not consistent between wieshark and airpcap
 */
void
airpcap_keys_check_w(GtkWidget *w, gpointer data);

void
on_keys_check_w_destroy (GtkWidget *w, gpointer user_data);

void
on_keys_check_cancel_bt_clicked (GtkWidget *button, gpointer user_data);

void
on_keys_check_ok_bt_clicked (GtkWidget *button, gpointer user_data);

void
on_keep_bt_clicked (GtkWidget *button, gpointer user_data);

void
on_merge_bt_clicked (GtkWidget *button, gpointer user_data);

void
on_import_bt_clicked (GtkWidget *button, gpointer user_data);

void
on_ignore_bt_clicked (GtkWidget *button, gpointer user_data);

#endif
