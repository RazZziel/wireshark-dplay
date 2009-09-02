/* packet-mp2t.c
 *
 * Routines for RFC 2250 MPEG2 (ISO/IEC 13818-1) Transport Stream dissection
 *
 * $Id$
 *
 * Copyright 2006, Erwin Rol <erwin@erwinrol.com>
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

#include <glib.h>
#include <epan/packet.h>

#include <stdio.h>
#include <string.h>

#include <epan/rtp_pt.h>
#include "packet-frame.h"

#include <epan/emem.h>
#include <epan/conversation.h>
#include <epan/expert.h>

/* The MPEG2 TS packet size */
#define MP2T_PACKET_SIZE 188
#define MP2T_SYNC_BYTE 0x47

static dissector_handle_t pes_handle;

static int proto_mp2t = -1;
static gint ett_mp2t = -1;
static gint ett_mp2t_header = -1;
static gint ett_mp2t_af = -1;

static int hf_mp2t_header = -1;
static int hf_mp2t_sync_byte = -1;
static int hf_mp2t_tei = -1;
static int hf_mp2t_pusi = -1;
static int hf_mp2t_tp = -1;
static int hf_mp2t_pid = -1;
static int hf_mp2t_tsc = -1;
static int hf_mp2t_afc = -1;
static int hf_mp2t_cc = -1;
static int hf_mp2t_cc_drop = -1;

#define MP2T_SYNC_BYTE_MASK	0xFF000000
#define MP2T_TEI_MASK		0x00800000
#define MP2T_PUSI_MASK		0x00400000
#define MP2T_TP_MASK		0x00200000
#define MP2T_PID_MASK		0x001FFF00
#define MP2T_TSC_MASK		0x000000C0
#define MP2T_AFC_MASK		0x00000030
#define MP2T_CC_MASK		0x0000000F

#define MP2T_SYNC_BYTE_SHIFT	24
#define MP2T_TEI_SHIFT		23
#define MP2T_PUSI_SHIFT		22
#define MP2T_TP_SHIFT		21
#define MP2T_PID_SHIFT		8
#define MP2T_TSC_SHIFT		6
#define MP2T_AFC_SHIFT		4
#define MP2T_CC_SHIFT		0

static int hf_mp2t_af = -1;
static int hf_mp2t_af_length = -1;
static int hf_mp2t_af_di = -1;
static int hf_mp2t_af_rai = -1;
static int hf_mp2t_af_espi = -1;
static int hf_mp2t_af_pcr_flag = -1;
static int hf_mp2t_af_opcr_flag = -1;
static int hf_mp2t_af_sp_flag = -1;
static int hf_mp2t_af_tpd_flag = -1;
static int hf_mp2t_af_afe_flag = -1;

#define MP2T_AF_DI_MASK 	0x80
#define MP2T_AF_RAI_MASK	0x40
#define MP2T_AF_ESPI_MASK	0x20
#define MP2T_AF_PCR_MASK	0x10
#define MP2T_AF_OPCR_MASK	0x08
#define MP2T_AF_SP_MASK		0x04
#define MP2T_AF_TPD_MASK	0x02
#define MP2T_AF_AFE_MASK	0x01

#define MP2T_AF_DI_SHIFT 	7
#define MP2T_AF_RAI_SHIFT	6
#define MP2T_AF_ESPI_SHIFT	5
#define MP2T_AF_PCR_SHIFT	4
#define MP2T_AF_OPCR_SHIFT	3
#define MP2T_AF_SP_SHIFT	2
#define MP2T_AF_TPD_SHIFT	1
#define MP2T_AF_AFE_SHIFT	0

static int hf_mp2t_af_pcr = -1;
static int hf_mp2t_af_opcr = -1;

static int hf_mp2t_af_sc = -1;

static int hf_mp2t_af_tpd_length = -1;
static int hf_mp2t_af_tpd = -1;

static int hf_mp2t_af_e_length = -1;
static int hf_mp2t_af_e_ltw_flag = -1;
static int hf_mp2t_af_e_pr_flag = -1;
static int hf_mp2t_af_e_ss_flag = -1;
static int hf_mp2t_af_e_reserved = -1;

#define MP2T_AF_E_LTW_FLAG_MASK	0x80 
#define MP2T_AF_E_PR_FLAG_MASK	0x40
#define MP2T_AF_E_SS_FLAG_MASK	0x20

static int hf_mp2t_af_e_reserved_bytes = -1;
static int hf_mp2t_af_stuffing_bytes = -1;

static int hf_mp2t_af_e_ltwv_flag = -1;
static int hf_mp2t_af_e_ltwo = -1;

static int hf_mp2t_af_e_pr_reserved = -1;
static int hf_mp2t_af_e_pr = -1;

static int hf_mp2t_af_e_st = -1;
static int hf_mp2t_af_e_dnau_32_30 = -1;
static int hf_mp2t_af_e_m_1 = -1;
static int hf_mp2t_af_e_dnau_29_15 = -1;
static int hf_mp2t_af_e_m_2 = -1;
static int hf_mp2t_af_e_dnau_14_0 = -1;
static int hf_mp2t_af_e_m_3 = -1;

static int hf_mp2t_payload = -1;
static int hf_mp2t_malformed_payload = -1;

static const value_string mp2t_sync_byte_vals[] = {
	{ MP2T_SYNC_BYTE, "Correct" },
	{ 0, NULL }
};


static const value_string mp2t_pid_vals[] = {
	{ 0x0000, "Program Association Table" },
	{ 0x0001, "Conditional Access Table" },
	{ 0x0002, "Transport Stream Description Table" },
	{ 0x0003, "Reserved" },
	{ 0x0004, "Reserved" },
	{ 0x0005, "Reserved" },
	{ 0x0006, "Reserved" },
	{ 0x0007, "Reserved" },
	{ 0x0008, "Reserved" },
	{ 0x0009, "Reserved" },
	{ 0x000A, "Reserved" },
	{ 0x000B, "Reserved" },
	{ 0x000C, "Reserved" },
	{ 0x000D, "Reserved" },
	{ 0x000E, "Reserved" },
	{ 0x000F, "Reserved" },
	{ 0x1FFF, "Null packet" },
	{ 0, NULL }
};

static const value_string mp2t_tsc_vals[] = {
	{ 0, "Not scrambled" },
	{ 1, "User-defined" },
	{ 2, "User-defined" },
	{ 3, "User-defined" },
	{ 0, NULL }
};

static const value_string mp2t_afc_vals[] = {
	{ 0, "Reserved" },
	{ 1, "Payload only" },
	{ 2, "Adaptation Field only" },
	{ 3, "Adaptation Field and Payload" },
	{ 0, NULL }
};

/* Data structure used for detecting CC drops
 *
 *  conversation
 *    |
 *    +-> mp2t_analysis_data
 *          |
 *          +-> pid_table (RB tree)
 *          |     |
 *          |     +-> pid_analysis_data (per pid)
 *          |     +-> pid_analysis_data
 *          |     +-> pid_analysis_data
 *          |
 *          +-> frame_table (RB tree)
 *                |
 *                +-> frame_analysis_data (only created if drop detected)
 *                      |
 *                      +-> ts_table (RB tree)
 *                            |
 *                            +-> pid_analysis_data (per TS subframe)
 *                            +-> pid_analysis_data
 *                            +-> pid_analysis_data
 */

typedef struct mp2t_analysis_data {

	/* This structure contains a tree containing data for the
	 * individual pid's, this is only used when packets are
	 * processed sequencially.
	 */
	emem_tree_t	*pid_table;

	/* When detecting a CC drop, store that information for the
	 * given frame.  This info is needed, when clicking around in
	 * wireshark, as the pid table data only makes sence during
	 * sequencial processing. The flag pinfo->fd->flags.visited is
	 * used to tell the difference.
	 *
	 */
	emem_tree_t	*frame_table;

	guint32 cc_drops;	/* Number of detected CC losses per conv */

} mp2t_analysis_data_t;

typedef struct pid_analysis_data {
	guint16 pid;
	gint16  cc_prev;  	/* Previous CC number */
} pid_analysis_data_t;

typedef struct frame_analysis_data {

	/* As each frame has several pid's, thus need a pid data
	 * structure per TS frame.
	 */
	emem_tree_t	*ts_table;

} frame_analysis_data_t;

static conversation_t *
get_the_conversation(packet_info *pinfo)
{
	conversation_t *conv = NULL;

	conv = find_conversation(pinfo->fd->num, &pinfo->src,
				 &pinfo->dst, pinfo->ptype,
				 pinfo->srcport, pinfo->destport, 0);

	if (conv == NULL) { /* Create a new conversation */
		conv = conversation_new(pinfo->fd->num, &pinfo->src,
					&pinfo->dst, pinfo->ptype,
					pinfo->srcport, pinfo->destport, 0);
	}
	return conv;
}

static mp2t_analysis_data_t *
init_mp2t_conversation_data(void)
{
	mp2t_analysis_data_t *mp2t_data = NULL;

	mp2t_data = se_alloc0(sizeof(struct mp2t_analysis_data));

	mp2t_data->pid_table =
		se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK,
					      "mp2t_pid_table");
	mp2t_data->frame_table =
		se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK,
					      "mp2t_frame_table");
	mp2t_data->cc_drops = 0;

	return mp2t_data;
}

static mp2t_analysis_data_t *
get_mp2t_conversation_data(conversation_t *conv)
{
	mp2t_analysis_data_t *mp2t_data = NULL;

	mp2t_data = conversation_get_proto_data(conv, proto_mp2t);
	if (!mp2t_data) {
		mp2t_data = init_mp2t_conversation_data();
		conversation_add_proto_data(conv, proto_mp2t, mp2t_data);
	}

	return mp2t_data;
}

static frame_analysis_data_t *
init_frame_analysis_data(mp2t_analysis_data_t *mp2t_data, packet_info *pinfo)
{
	frame_analysis_data_t *frame_data = NULL;

	frame_data = se_alloc0(sizeof(struct frame_analysis_data));
	frame_data->ts_table =
	  se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK,
					"mp2t_frame_pid_table");
	/* Insert into mp2t tree */
	se_tree_insert32(mp2t_data->frame_table, pinfo->fd->num,
			 (void *)frame_data);

	return frame_data;
}


static frame_analysis_data_t *
get_frame_analysis_data(mp2t_analysis_data_t *mp2t_data, packet_info *pinfo)
{
	frame_analysis_data_t *frame_data = NULL;
	frame_data = se_tree_lookup32(mp2t_data->frame_table, pinfo->fd->num);
	return frame_data;
}

static pid_analysis_data_t *
get_pid_analysis(guint32 pid, conversation_t *conv)
{

	pid_analysis_data_t  *pid_data  = NULL;
	mp2t_analysis_data_t *mp2t_data = NULL;
	mp2t_data = get_mp2t_conversation_data(conv);

	pid_data = se_tree_lookup32(mp2t_data->pid_table, pid);
	if (!pid_data) {
		pid_data          = se_alloc0(sizeof(struct pid_analysis_data));
		pid_data->cc_prev = -1;
		pid_data->pid     = pid;

		se_tree_insert32(mp2t_data->pid_table, pid, (void *)pid_data);
	}
	return pid_data;
}

#define KEY(pid, cc) ((pid << 4)|cc)

static void
detect_cc_drops(tvbuff_t *tvb, proto_tree *tree, packet_info *pinfo,
		guint32 pid, gint32 cc_curr, conversation_t *conv)
{
	gint32 cc_prev = -1;
	pid_analysis_data_t   *pid_data   = NULL;
	pid_analysis_data_t   *ts_data    = NULL;
	mp2t_analysis_data_t  *mp2t_data  = NULL;
	frame_analysis_data_t *frame_data = NULL;
	proto_item            *flags_item;

	guint32 detected_drop = 0;

	mp2t_data = get_mp2t_conversation_data(conv);

	/* The initial sequencial processing stage */
	if (!pinfo->fd->flags.visited) {

		/* This is the sequencial processing stage */
		pid_data = get_pid_analysis(pid, conv);

		cc_prev = pid_data->cc_prev;
		pid_data->cc_prev = cc_curr;

		/* Null packet always have a CC value equal 0 */
		if (pid == 0x1fff)
			return;

		/* Its allowed that (cc_prev == cc_curr) if adaptation field */
		if (cc_prev == cc_curr)
			return;

		/* Have not seen this pid before */
		if (cc_prev == -1)
			return;

		/* Detect if CC is not increasing by one all the time */
		if (cc_curr != ((cc_prev+1) & MP2T_CC_MASK)) {
			detected_drop = 1;
			mp2t_data->cc_drops++;
		}
	}

	/* Save the info about the dropped packet */
	if (detected_drop && !pinfo->fd->flags.visited) {

		/* Lookup frame data, contains TS pid data objects */
		frame_data = get_frame_analysis_data(mp2t_data, pinfo);
		if (!frame_data)
			frame_data = init_frame_analysis_data(mp2t_data, pinfo);

		/* Create and store a new TS frame pid_data object.
		   This indicate that we have a drop
		 */
		ts_data = se_alloc0(sizeof(struct pid_analysis_data));
		ts_data->cc_prev = cc_prev;
		ts_data->pid = pid;
		se_tree_insert32(frame_data->ts_table, KEY(pid, cc_curr),
				 (void *)ts_data);
	}

	/* See if we stored info about drops */
	if (pinfo->fd->flags.visited) {

		/* Lookup frame data, contains TS pid data objects */
		frame_data = get_frame_analysis_data(mp2t_data, pinfo);
		if (!frame_data)
			return; /* No stored frame data -> no drops*/
		else {
			ts_data = se_tree_lookup32(frame_data->ts_table,
						   KEY(pid, cc_curr));
			if (ts_data)
				detected_drop = 1;
		}

	}

	/* Add info to the proto tree about drops */
	if (detected_drop) {

		flags_item =
			proto_tree_add_none_format(
				tree, hf_mp2t_cc_drop, tvb, 0, 0,
				"Detected missing CC frame before this"
				" (accumulated CC loss count:%d)",
				mp2t_data->cc_drops
				);

		PROTO_ITEM_SET_GENERATED(flags_item);
		expert_add_info_format(pinfo, flags_item, PI_MALFORMED,
				       PI_ERROR, "Detected CC loss");

	}
}


static gint
dissect_tsp(tvbuff_t *tvb, volatile gint offset, packet_info *pinfo,
	    proto_tree *tree, conversation_t *conv)
{
	guint32 header;
	guint afc;
	gint start_offset = offset;
	volatile gint payload_len;

	guint32 pid;
	guint32 cc;

	proto_item *ti = NULL;
	proto_item *hi = NULL;
	proto_tree *mp2t_tree = NULL;
	proto_tree *mp2t_header_tree = NULL;
	proto_tree *mp2t_af_tree = NULL;

	ti = proto_tree_add_item( tree, proto_mp2t, tvb, offset, MP2T_PACKET_SIZE, FALSE );
	mp2t_tree = proto_item_add_subtree( ti, ett_mp2t );
	
	header = tvb_get_ntohl(tvb, offset);

	pid = (header & MP2T_PID_MASK) >> MP2T_PID_SHIFT;
	cc  = (header & MP2T_CC_MASK)  >> MP2T_CC_SHIFT;
	proto_item_append_text(ti, " PID=0x%x CC=%d", pid, cc);

	detect_cc_drops(tvb, tree, pinfo, pid, cc, conv);

	hi = proto_tree_add_item( mp2t_tree, hf_mp2t_header, tvb, offset, 4, FALSE);
	mp2t_header_tree = proto_item_add_subtree( hi, ett_mp2t_header );

	proto_tree_add_item( mp2t_header_tree, hf_mp2t_sync_byte, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_tei, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_pusi, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_tp, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_pid, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_tsc, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_afc, tvb, offset, 4, FALSE);
	proto_tree_add_item( mp2t_header_tree, hf_mp2t_cc, tvb, offset, 4, FALSE);
	offset += 4;

	afc = (header & MP2T_AFC_MASK) >> MP2T_AFC_SHIFT;

	if (afc == 2 || afc == 3) 
	{
		gint af_start_offset = offset;
	
		guint8 af_length;
		guint8 af_flags;
		gint stuffing_len;


		af_length = tvb_get_guint8(tvb, offset);

		proto_tree_add_item( mp2t_tree, hf_mp2t_af_length, tvb, offset, 1, FALSE);
		offset += 1;
		/* fix issues where afc==3 but af_length==0 
		 *  Adaptaion field...spec section 2.4.3.5: The value 0 is for inserting a single 
		 *  stuffing byte in a Transport Stream packet. When the adaptation_field_control 
		 *  value is '11', the value of the adaptation_field_length shall be in the range 0 to 182.
		 */
		if (af_length > 0 ) {
			hi = proto_tree_add_item( mp2t_tree, hf_mp2t_af, tvb, offset, af_length, FALSE);
			mp2t_af_tree = proto_item_add_subtree( hi, ett_mp2t_af );
	
			af_flags = tvb_get_guint8(tvb, offset);
	
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_di, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_rai, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_espi, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_pcr_flag, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_opcr_flag, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_sp_flag, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_tpd_flag, tvb, offset, 1, FALSE);
			proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_afe_flag, tvb, offset, 1, FALSE);
	
			offset += 1;
	
			if (af_flags &  MP2T_AF_PCR_MASK) {
				guint64 pcr_base = 0;
				guint32 pcr_ext = 0;
				guint8 tmp;

				tmp = tvb_get_guint8(tvb, offset);
				pcr_base = (pcr_base << 8) | tmp;
				offset += 1;
			
				tmp = tvb_get_guint8(tvb, offset);
				pcr_base = (pcr_base << 8) | tmp;
				offset += 1;
				
				tmp = tvb_get_guint8(tvb, offset);
				pcr_base = (pcr_base << 8) | tmp;
				offset += 1;
	
				tmp = tvb_get_guint8(tvb, offset);
				pcr_base = (pcr_base << 8) | tmp;
				offset += 1;

				tmp = tvb_get_guint8(tvb, offset);
				pcr_base = (pcr_base << 1) | ((tmp >> 7) & 0x01);
				pcr_ext = (tmp & 0x01);
				offset += 1;

				tmp = tvb_get_guint8(tvb, offset);
				pcr_ext = (pcr_ext << 8) | tmp;
				offset += 1;

				proto_tree_add_none_format(mp2t_af_tree, hf_mp2t_af_pcr, tvb, offset - 6, 6, 
						"Program Clock Reference: base(%" G_GINT64_MODIFIER "u) * 300 + ext(%u) = %" G_GINT64_MODIFIER "u", 
						pcr_base, pcr_ext, pcr_base * 300 + pcr_ext);
			}

			if (af_flags &  MP2T_AF_OPCR_MASK) {
				guint64 opcr_base = 0;
				guint32 opcr_ext = 0;
				guint8 tmp = 0;

				tmp = tvb_get_guint8(tvb, offset);
				opcr_base = (opcr_base << 8) | tmp;
				offset += 1;
			
				tmp = tvb_get_guint8(tvb, offset);
				opcr_base = (opcr_base << 8) | tmp;
				offset += 1;
			
				tmp = tvb_get_guint8(tvb, offset);
				opcr_base = (opcr_base << 8) | tmp;
				offset += 1;
	
				tmp = tvb_get_guint8(tvb, offset);
				opcr_base = (opcr_base << 8) | tmp;
				offset += 1;

				tmp = tvb_get_guint8(tvb, offset);
				opcr_base = (opcr_base << 1) | ((tmp >> 7) & 0x01);
				opcr_ext = (tmp & 0x01);
				offset += 1;

				tmp = tvb_get_guint8(tvb, offset);
				opcr_ext = (opcr_ext << 8) | tmp;
				offset += 1;

				proto_tree_add_none_format(mp2t_af_tree, hf_mp2t_af_opcr, tvb, offset - 6, 6, 
						"Original Program Clock Reference: base(%" G_GINT64_MODIFIER "u) * 300 + ext(%u) = %" G_GINT64_MODIFIER "u", 
						opcr_base, opcr_ext, opcr_base * 300 + opcr_ext);
	
				offset += 6;
			}
	
			if (af_flags &  MP2T_AF_SP_MASK) {
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_sc, tvb, offset, 1, FALSE);
				offset += 1;
			}

			if (af_flags &  MP2T_AF_TPD_MASK) {
				guint8 tpd_len;
			
				tpd_len = tvb_get_guint8(tvb, offset);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_tpd_length, tvb, offset, 1, FALSE);
				offset += 1;

				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_tpd, tvb, offset, tpd_len, FALSE);
				offset += tpd_len;
			}

			if (af_flags &  MP2T_AF_AFE_MASK) {
				guint8 e_len;
				guint8 e_flags;
				gint e_start_offset = offset;
				gint reserved_len = 0;

				e_len = tvb_get_guint8(tvb, offset);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_length, tvb, offset, 1, FALSE);
				offset += 1;

				e_flags = tvb_get_guint8(tvb, offset);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_ltw_flag, tvb, offset, 1, FALSE);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_pr_flag, tvb, offset, 1, FALSE);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_ss_flag, tvb, offset, 1, FALSE);
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_reserved, tvb, offset, 1, FALSE);			
				offset += 1;
			
				if (e_flags & MP2T_AF_E_LTW_FLAG_MASK) {
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_ltwv_flag, tvb, offset, 2, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_ltwo, tvb, offset, 2, FALSE);
					offset += 2;
				}

				if (e_flags & MP2T_AF_E_PR_FLAG_MASK) {
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_pr_reserved, tvb, offset, 3, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_pr, tvb, offset, 3, FALSE);
					offset += 3;
				}

				if (e_flags & MP2T_AF_E_SS_FLAG_MASK) {
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_st, tvb, offset, 1, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_dnau_32_30, tvb, offset, 1, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_m_1, tvb, offset, 1, FALSE);
					offset += 1;
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_dnau_29_15, tvb, offset, 2, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_m_2, tvb, offset, 2, FALSE);
					offset += 2;
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_dnau_14_0, tvb, offset, 2, FALSE);
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_m_3, tvb, offset, 2, FALSE);
					offset += 2;
				}

				reserved_len = (e_len + 1) - (offset - e_start_offset);
				if (reserved_len > 0) {
					proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_e_reserved_bytes, tvb, offset, reserved_len, FALSE);
					offset += reserved_len;
				}
			}

			stuffing_len = (af_length + 1) - (offset - af_start_offset);
			if (stuffing_len > 0) {
				proto_tree_add_item( mp2t_af_tree, hf_mp2t_af_stuffing_bytes, tvb, offset, stuffing_len, FALSE);
				offset += stuffing_len;
			}
		}
	}

	payload_len = MP2T_PACKET_SIZE - (offset - start_offset);
	if (payload_len > 0) {
		if (afc == 2) {	/* AF only */
			/* Packet is malformed */
			proto_tree_add_item( mp2t_tree, hf_mp2t_malformed_payload, tvb, offset, payload_len, FALSE);
			offset += payload_len;
		} else {
			/* Check to make sure if we are not at end of payload, if we have less than 3 bytes, the tvb_get_ntoh24 fails. */
			if (payload_len >=3 ) {
				if (tvb_get_ntoh24(tvb, offset) == 0x000001) {
					tvbuff_t *next_tvb = tvb_new_subset(tvb, offset, payload_len, payload_len);
					const char *saved_proto = pinfo->current_proto;

					TRY {
						call_dissector(pes_handle, next_tvb, pinfo, mp2t_tree);
					}

					/*
					 Don't stop processing TS packets if somebody threw
					 BoundsError, which means that dissecting the payload found
					 that the packet was cut off by before the end of the
					 payload.  This is very likely as this protocol splits the
					 media stream up into chunks of MP2T_PACKET_SIZE.
					*/
					CATCH2(BoundsError, ReportedBoundsError) {
						show_exception(next_tvb, pinfo, tree, EXCEPT_CODE, GET_MESSAGE);
						pinfo->current_proto = saved_proto;
					}

					ENDTRY;
				} else {
					proto_tree_add_item( mp2t_tree, hf_mp2t_payload, tvb, offset, payload_len, FALSE);
				}
			} else {
				proto_tree_add_item( mp2t_tree, hf_mp2t_payload, tvb, offset, payload_len, FALSE);
			}
			offset += payload_len;
		}
	}

	return offset;
}


static void
dissect_mp2t( tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree )
{
	guint offset = 0;
	conversation_t *conv;
	conv = get_the_conversation(pinfo);

	while ( tvb_reported_length_remaining(tvb, offset) >= MP2T_PACKET_SIZE ) {
		offset = dissect_tsp(tvb, offset, pinfo, tree, conv);
	}
}

static gboolean
heur_dissect_mp2t( tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree )
{
	guint offset = 0;

	if (tvb_length_remaining(tvb, offset) % MP2T_PACKET_SIZE) {
		return FALSE;
	} else {
		while (tvb_length_remaining(tvb, offset)) {
			if (tvb_get_guint8(tvb, offset) != MP2T_SYNC_BYTE)
				return FALSE;
			offset += MP2T_PACKET_SIZE;
		}
	}

	dissect_mp2t(tvb, pinfo, tree);
	return TRUE;
}

void
proto_register_mp2t(void)
{
	static hf_register_info hf[] = { 
		{ &hf_mp2t_header, {
			"Header", "mp2t.header",
			FT_UINT32, BASE_HEX, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_sync_byte, {
			"Sync Byte", "mp2t.sync_byte",
			FT_UINT32, BASE_HEX, VALS(mp2t_sync_byte_vals), MP2T_SYNC_BYTE_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_tei, { 
			"Transport Error Indicator", "mp2t.tei",
			FT_UINT32, BASE_DEC, NULL, MP2T_TEI_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_pusi, {
			"Payload Unit Start Indicator", "mp2t.pusi",
			FT_UINT32, BASE_DEC, NULL, MP2T_PUSI_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_tp, {
			"Transport Priority", "mp2t.tp",
			FT_UINT32, BASE_DEC, NULL, MP2T_TP_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_pid, {
			"PID", "mp2t.pid",
			FT_UINT32, BASE_HEX, VALS(mp2t_pid_vals), MP2T_PID_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_tsc, {
			"Transport Scrambling Control", "mp2t.tsc",
			FT_UINT32, BASE_HEX, VALS(mp2t_tsc_vals), MP2T_TSC_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_afc, {
			"Adaption Field Control", "mp2t.afc",
			FT_UINT32, BASE_HEX, VALS(mp2t_afc_vals) , MP2T_AFC_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_cc, {
			"Continuity Counter", "mp2t.cc",
			FT_UINT32, BASE_DEC, NULL, MP2T_CC_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_cc_drop, {
			"Continuity Counter Drops", "mp2t.cc.drop",
			FT_NONE, BASE_NONE, NULL, 0x0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af, {
			"Adaption field", "mp2t.af",
			FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_length, {
			"Adaptation Field Length", "mp2t.af.length",
			FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_di, {
			"Discontinuity Indicator", "mp2t.af.di",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_DI_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_rai, {
			"Random Access Indicator", "mp2t.af.rai",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_RAI_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_espi, {
			"Elementary Stream Priority Indicator", "mp2t.af.espi",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_ESPI_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_pcr_flag, {
			"PCR Flag", "mp2t.af.pcr_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_PCR_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_opcr_flag, {
			"OPCR Flag", "mp2t.af.opcr_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_OPCR_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_sp_flag, {
			"Splicing Point Flag", "mp2t.af.sp_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_SP_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_tpd_flag, {
			"Transport Private Data Flag", "mp2t.af.tpd_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_TPD_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_afe_flag, {
			"Adaptation Field Extension Flag", "mp2t.af.afe_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_AFE_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_pcr, {
			"Program Clock Reference", "mp2t.af.pcr",
			FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_opcr, {
			"Original Program Clock Reference", "mp2t.af.opcr",
			FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_sc, {
			"Splice Countdown", "mp2t.af.sc",
			FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_tpd_length, {
			"Transport Private Data Length", "mp2t.af.tpd_length",
			FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_tpd, {
			"Transport Private Data", "mp2t.af.tpd",
			FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_length, {
			"Adaptation Field Extension Length", "mp2t.af.e_length",
			FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_ltw_flag, {
			"LTW Flag", "mp2t.af.e.ltw_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_E_LTW_FLAG_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_pr_flag, {
			"Piecewise Rate Flag", "mp2t.af.e.pr_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_E_PR_FLAG_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_ss_flag, {
			"Seamless Splice Flag", "mp2t.af.e.ss_flag",
			FT_UINT8, BASE_DEC, NULL, MP2T_AF_E_SS_FLAG_MASK, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_reserved, {
			"Reserved", "mp2t.af.e.reserved",
			FT_UINT8, BASE_DEC, NULL, 0x1F, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_reserved_bytes, {
			"Reserved", "mp2t.af.e.reserved_bytes",
			FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_stuffing_bytes, {
			"Stuffing", "mp2t.af.stuffing_bytes",
			FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_ltwv_flag, {
			"LTW Valid Flag", "mp2t.af.e.ltwv_flag",
			FT_UINT16, BASE_DEC, NULL, 0x8000, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_ltwo, {
			"LTW Offset", "mp2t.af.e.ltwo",
			FT_UINT16, BASE_DEC, NULL, 0x7FFF, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_pr_reserved, {
			"Reserved", "mp2t.af.e.pr_reserved",
			FT_UINT24, BASE_DEC, NULL, 0xC00000, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_pr, {
			"Piecewise Rate", "mp2t.af.e.pr",
			FT_UINT24, BASE_DEC, NULL, 0x3FFFFF, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_st, {
			"Splice Type", "mp2t.af.e.st",
			FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_dnau_32_30, {
			"DTS Next AU[32...30]", "mp2t.af.e.dnau_32_30",
			FT_UINT8, BASE_DEC, NULL, 0x0E, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_m_1, {
			"Marker Bit", "mp2t.af.e.m_1",
			FT_UINT8, BASE_DEC, NULL, 0x01, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_dnau_29_15, {
			"DTS Next AU[29...15]", "mp2t.af.e.dnau_29_15",
			FT_UINT16, BASE_DEC, NULL, 0xFFFE, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_m_2, {
			"Marker Bit", "mp2t.af.e.m_2",
			FT_UINT16, BASE_DEC, NULL, 0x0001, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_dnau_14_0, {
			"DTS Next AU[14...0]", "mp2t.af.e.dnau_14_0",
			FT_UINT16, BASE_DEC, NULL, 0xFFFE, NULL, HFILL
		} } ,
		{ &hf_mp2t_af_e_m_3, {
			"Marker Bit", "mp2t.af.e.m_3",
			FT_UINT16, BASE_DEC, NULL, 0x0001, NULL, HFILL
		} } ,
		{ &hf_mp2t_payload, {
			"Payload", "mp2t.payload",
			FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL
		} } ,
		{ &hf_mp2t_malformed_payload, {
			"Malformed Payload", "mp2t.malformed_payload",
			FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL
		} }
	};

	static gint *ett[] =
	{
		&ett_mp2t,
		&ett_mp2t_header,
		&ett_mp2t_af
	};

	proto_mp2t = proto_register_protocol("ISO/IEC 13818-1", "MP2T", "mp2t");
	proto_register_field_array(proto_mp2t, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}



void
proto_reg_handoff_mp2t(void)
{
	dissector_handle_t mp2t_handle;

	heur_dissector_add("udp", heur_dissect_mp2t, proto_mp2t);

	mp2t_handle = create_dissector_handle(dissect_mp2t, proto_mp2t);
	dissector_add("rtp.pt", PT_MP2T, mp2t_handle);
	dissector_add_handle("udp.port", mp2t_handle);  /* for decode-as */

	pes_handle = find_dissector("mpeg-pes");
}

