/* packet-tcp.c
 * Routines for TCP packet disassembly
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <epan/in_cksum.h>

#include <epan/packet.h>
#include <epan/addr_resolv.h>
#include <epan/ipproto.h>
#include <epan/ip_opts.h>
#include <epan/follow.h>
#include <epan/prefs.h>
#include <epan/emem.h>
#include "packet-tcp.h"
#include "packet-frame.h"
#include <epan/conversation.h>
#include <epan/reassemble.h>
#include <epan/tap.h>
#include <epan/slab.h>
#include <epan/expert.h>

static int tcp_tap = -1;

/* Place TCP summary in proto tree */
static gboolean tcp_summary_in_tree = TRUE;

/*
 * Flag to control whether to check the TCP checksum.
 *
 * In at least some Solaris network traces, there are packets with bad
 * TCP checksums, but the traffic appears to indicate that the packets
 * *were* received; the packets were probably sent by the host on which
 * the capture was being done, on a network interface to which
 * checksumming was offloaded, so that DLPI supplied an un-checksummed
 * packet to the capture program but a checksummed packet got put onto
 * the wire.
 */
static gboolean tcp_check_checksum = FALSE;

extern FILE* data_out_file;

static int proto_tcp = -1;
static int hf_tcp_srcport = -1;
static int hf_tcp_dstport = -1;
static int hf_tcp_port = -1;
static int hf_tcp_stream = -1;
static int hf_tcp_seq = -1;
static int hf_tcp_nxtseq = -1;
static int hf_tcp_ack = -1;
static int hf_tcp_hdr_len = -1;
static int hf_tcp_flags = -1;
static int hf_tcp_flags_cwr = -1;
static int hf_tcp_flags_ecn = -1;
static int hf_tcp_flags_urg = -1;
static int hf_tcp_flags_ack = -1;
static int hf_tcp_flags_push = -1;
static int hf_tcp_flags_reset = -1;
static int hf_tcp_flags_syn = -1;
static int hf_tcp_flags_fin = -1;
static int hf_tcp_window_size = -1;
static int hf_tcp_checksum = -1;
static int hf_tcp_checksum_bad = -1;
static int hf_tcp_checksum_good = -1;
static int hf_tcp_len = -1;
static int hf_tcp_urgent_pointer = -1;
static int hf_tcp_analysis_flags = -1;
static int hf_tcp_analysis_bytes_in_flight = -1;
static int hf_tcp_analysis_acks_frame = -1;
static int hf_tcp_analysis_ack_rtt = -1;
static int hf_tcp_analysis_rto = -1;
static int hf_tcp_analysis_rto_frame = -1;
static int hf_tcp_analysis_retransmission = -1;
static int hf_tcp_analysis_fast_retransmission = -1;
static int hf_tcp_analysis_out_of_order = -1;
static int hf_tcp_analysis_reused_ports = -1;
static int hf_tcp_analysis_lost_packet = -1;
static int hf_tcp_analysis_ack_lost_packet = -1;
static int hf_tcp_analysis_window_update = -1;
static int hf_tcp_analysis_window_full = -1;
static int hf_tcp_analysis_keep_alive = -1;
static int hf_tcp_analysis_keep_alive_ack = -1;
static int hf_tcp_analysis_duplicate_ack = -1;
static int hf_tcp_analysis_duplicate_ack_num = -1;
static int hf_tcp_analysis_duplicate_ack_frame = -1;
static int hf_tcp_analysis_zero_window = -1;
static int hf_tcp_analysis_zero_window_probe = -1;
static int hf_tcp_analysis_zero_window_probe_ack = -1;
static int hf_tcp_continuation_to = -1;
static int hf_tcp_pdu_time = -1;
static int hf_tcp_pdu_size = -1;
static int hf_tcp_pdu_last_frame = -1;
static int hf_tcp_reassembled_in = -1;
static int hf_tcp_segments = -1;
static int hf_tcp_segment = -1;
static int hf_tcp_segment_overlap = -1;
static int hf_tcp_segment_overlap_conflict = -1;
static int hf_tcp_segment_multiple_tails = -1;
static int hf_tcp_segment_too_long_fragment = -1;
static int hf_tcp_segment_error = -1;
static int hf_tcp_options = -1;
static int hf_tcp_option_mss = -1;
static int hf_tcp_option_mss_val = -1;
static int hf_tcp_option_wscale = -1;
static int hf_tcp_option_wscale_val = -1;
static int hf_tcp_option_sack_perm = -1;
static int hf_tcp_option_sack = -1;
static int hf_tcp_option_sack_sle = -1;
static int hf_tcp_option_sack_sre = -1;
static int hf_tcp_option_echo = -1;
static int hf_tcp_option_echo_reply = -1;
static int hf_tcp_option_time_stamp = -1;
static int hf_tcp_option_cc = -1;
static int hf_tcp_option_ccnew = -1;
static int hf_tcp_option_ccecho = -1;
static int hf_tcp_option_md5 = -1;
static int hf_tcp_option_qs = -1;
static int hf_tcp_ts_relative = -1;
static int hf_tcp_ts_delta = -1;
static int hf_tcp_option_scps = -1;
static int hf_tcp_option_scps_vector = -1;
static int hf_tcp_option_scps_binding = -1;
static int hf_tcp_scpsoption_flags_bets = -1;
static int hf_tcp_scpsoption_flags_snack1 = -1;
static int hf_tcp_scpsoption_flags_snack2 = -1;
static int hf_tcp_scpsoption_flags_compress = -1;
static int hf_tcp_scpsoption_flags_nlts = -1;
static int hf_tcp_scpsoption_flags_resv1 = -1;
static int hf_tcp_scpsoption_flags_resv2 = -1;
static int hf_tcp_scpsoption_flags_resv3 = -1;
static int hf_tcp_option_snack = -1;
static int hf_tcp_option_snack_offset = -1;
static int hf_tcp_option_snack_size = -1;
static int hf_tcp_option_snack_le = -1;
static int hf_tcp_option_snack_re = -1;
static int hf_tcp_proc_src_uid = -1;
static int hf_tcp_proc_src_pid = -1;
static int hf_tcp_proc_src_uname = -1;
static int hf_tcp_proc_src_cmd = -1;
static int hf_tcp_proc_dst_uid = -1;
static int hf_tcp_proc_dst_pid = -1;
static int hf_tcp_proc_dst_uname = -1;
static int hf_tcp_proc_dst_cmd = -1;

static gint ett_tcp = -1;
static gint ett_tcp_flags = -1;
static gint ett_tcp_options = -1;
static gint ett_tcp_option_sack = -1;
static gint ett_tcp_option_scps = -1;
static gint ett_tcp_option_scps_extended = -1;
static gint ett_tcp_analysis = -1;
static gint ett_tcp_analysis_faults = -1;
static gint ett_tcp_timestamps = -1;
static gint ett_tcp_segments = -1;
static gint ett_tcp_segment  = -1;
static gint ett_tcp_checksum = -1;
static gint ett_tcp_process_info = -1;


/* not all of the hf_fields below make sense for TCP but we have to provide
   them anyways to comply with the api (which was aimed for ip fragment
   reassembly) */
static const fragment_items tcp_segment_items = {
	&ett_tcp_segment,
	&ett_tcp_segments,
	&hf_tcp_segments,
	&hf_tcp_segment,
	&hf_tcp_segment_overlap,
	&hf_tcp_segment_overlap_conflict,
	&hf_tcp_segment_multiple_tails,
	&hf_tcp_segment_too_long_fragment,
	&hf_tcp_segment_error,
	&hf_tcp_reassembled_in,
	"Segments"
};

static dissector_table_t subdissector_table;
static heur_dissector_list_t heur_subdissector_list;
static dissector_handle_t data_handle;

/* TCP structs and definitions */

/* **************************************************************************

 * RTT and reltive sequence numbers.
 * **************************************************************************/
static gboolean tcp_analyze_seq = TRUE;
static gboolean tcp_relative_seq = TRUE;
static gboolean tcp_track_bytes_in_flight = TRUE;
static gboolean tcp_calculate_ts = FALSE;

/* SLAB allocator for tcp_unacked structures
 */
SLAB_ITEM_TYPE_DEFINE(tcp_unacked_t)
static SLAB_FREE_LIST_DEFINE(tcp_unacked_t)
#define TCP_UNACKED_NEW(fi)					\
	SLAB_ALLOC(fi, tcp_unacked_t)
#define TCP_UNACKED_FREE(fi)					\
	SLAB_FREE(fi, tcp_unacked_t)


#define TCP_A_RETRANSMISSION		0x0001
#define TCP_A_LOST_PACKET		0x0002
#define TCP_A_ACK_LOST_PACKET		0x0004
#define TCP_A_KEEP_ALIVE		0x0008
#define TCP_A_DUPLICATE_ACK		0x0010
#define TCP_A_ZERO_WINDOW		0x0020
#define TCP_A_ZERO_WINDOW_PROBE		0x0040
#define TCP_A_ZERO_WINDOW_PROBE_ACK	0x0080
#define TCP_A_KEEP_ALIVE_ACK		0x0100
#define TCP_A_OUT_OF_ORDER		0x0200
#define TCP_A_FAST_RETRANSMISSION	0x0400
#define TCP_A_WINDOW_UPDATE		0x0800
#define TCP_A_WINDOW_FULL		0x1000
#define TCP_A_REUSED_PORTS		0x2000


static void
process_tcp_payload(tvbuff_t *tvb, volatile int offset, packet_info *pinfo,
	proto_tree *tree, proto_tree *tcp_tree, int src_port, int dst_port,
	guint32 seq, guint32 nxtseq, gboolean is_tcp_segment,
	struct tcp_analysis *tcpd);


struct tcp_analysis *
init_tcp_conversation_data(packet_info *pinfo)
{
	struct tcp_analysis *tcpd=NULL;

	/* Initialize the tcp protocol datat structure to add to the tcp conversation */
	tcpd=se_alloc0(sizeof(struct tcp_analysis));
	memset(&tcpd->flow1, 0, sizeof(tcp_flow_t));
	memset(&tcpd->flow2, 0, sizeof(tcp_flow_t));
	tcpd->flow1.win_scale=-1;
	tcpd->flow1.multisegment_pdus=se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK, "tcp_multisegment_pdus");
	tcpd->flow1.username = NULL;
	tcpd->flow1.command = NULL;
	tcpd->flow2.win_scale=-1;
	tcpd->flow2.multisegment_pdus=se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK, "tcp_multisegment_pdus");
	tcpd->flow2.username = NULL;
	tcpd->flow2.command = NULL;
	tcpd->acked_table=se_tree_create_non_persistent(EMEM_TREE_TYPE_RED_BLACK, "tcp_analyze_acked_table");
	tcpd->ts_first.secs=pinfo->fd->abs_ts.secs;
	tcpd->ts_first.nsecs=pinfo->fd->abs_ts.nsecs;
	tcpd->ts_prev.secs=pinfo->fd->abs_ts.secs;
	tcpd->ts_prev.nsecs=pinfo->fd->abs_ts.nsecs;

	return tcpd;
}

conversation_t *
get_tcp_conversation(packet_info *pinfo)
{
	conversation_t *conv=NULL;

	/* Have we seen this conversation before? */
	if( (conv=find_conversation(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype, pinfo->srcport, pinfo->destport, 0)) == NULL){
		/* No this is a new conversation. */
		conv=conversation_new(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype, pinfo->srcport, pinfo->destport, 0);
	}
	return conv;
}

struct tcp_analysis *
get_tcp_conversation_data(conversation_t *conv, packet_info *pinfo)
{
	int direction;
	struct tcp_analysis *tcpd=NULL;

	/* Did the caller supply the conversation pointer? */
	if( conv==NULL )
	        conv = get_tcp_conversation(pinfo);

	/* Get the data for this conversation */
	tcpd=conversation_get_proto_data(conv, proto_tcp);

	/* If the conversation was just created or it matched a
	 * conversation with template options, tcpd will not
	 * have been initialized. So, initialize
	 * a new tcpd structure for the conversation.
	 */
	if (!tcpd) {
		tcpd = init_tcp_conversation_data(pinfo);
		conversation_add_proto_data(conv, proto_tcp, tcpd);
	}

	if (!tcpd) {
	  return NULL;
	}

	/* check direction and get ua lists */
	direction=CMP_ADDRESS(&pinfo->src, &pinfo->dst);
	/* if the addresses are equal, match the ports instead */
	if(direction==0) {
		direction= (pinfo->srcport > pinfo->destport) ? 1 : -1;
	}
	if(direction>=0){
		tcpd->fwd=&(tcpd->flow1);
		tcpd->rev=&(tcpd->flow2);
	} else {
		tcpd->fwd=&(tcpd->flow2);
		tcpd->rev=&(tcpd->flow1);
	}

	tcpd->ta=NULL;
	return tcpd;
}

/* Attach process info to a flow */
/* XXX - We depend on the TCP dissector finding the conversation first */
void
add_tcp_process_info(guint32 frame_num, address *local_addr, address *remote_addr, guint16 local_port, guint16 remote_port, guint32 uid, guint32 pid, gchar *username, gchar *command) {
	conversation_t *conv;
	struct tcp_analysis *tcpd;
	tcp_flow_t *flow = NULL;

	conv = find_conversation(frame_num, local_addr, remote_addr, PT_TCP, local_port, remote_port, 0);
	if (!conv) {
		return;
	}

	tcpd = conversation_get_proto_data(conv, proto_tcp);
	if (!tcpd) {
		return;
	}

	if (CMP_ADDRESS(local_addr, &conv->key_ptr->addr1) == 0 && local_port == conv->key_ptr->port1) {
		flow = &tcpd->flow1;
	} else if (CMP_ADDRESS(remote_addr, &conv->key_ptr->addr1) == 0 && remote_port == conv->key_ptr->port1) {
		flow = &tcpd->flow2;
	}
	if (!flow || flow->command) {
		return;
	}

	flow->process_uid = uid;
	flow->process_pid = pid;
	flow->username = se_strdup(username);
	flow->command = se_strdup(command);
}


/* Calculate the timestamps relative to this conversation */
static void
tcp_calculate_timestamps(packet_info *pinfo, struct tcp_analysis *tcpd,
			struct tcp_per_packet_data_t *tcppd)
{
	if( !tcppd ) {
		tcppd = se_alloc(sizeof(struct tcp_per_packet_data_t));
		p_add_proto_data(pinfo->fd, proto_tcp, tcppd);
	}

	if (!tcpd)
		return;

	nstime_delta(&tcppd->ts_del, &pinfo->fd->abs_ts, &tcpd->ts_prev);

	tcpd->ts_prev.secs=pinfo->fd->abs_ts.secs;
	tcpd->ts_prev.nsecs=pinfo->fd->abs_ts.nsecs;
}

/* Add a subtree with the timestamps relative to this conversation */
static void
tcp_print_timestamps(packet_info *pinfo, tvbuff_t *tvb, proto_tree *parent_tree, struct tcp_analysis *tcpd, struct tcp_per_packet_data_t *tcppd)
{
	proto_item	*item;
	proto_tree	*tree;
	nstime_t	ts;

	if (!tcpd)
		return;

	item=proto_tree_add_text(parent_tree, tvb, 0, 0, "Timestamps");
	PROTO_ITEM_SET_GENERATED(item);
	tree=proto_item_add_subtree(item, ett_tcp_timestamps);

	nstime_delta(&ts, &pinfo->fd->abs_ts, &tcpd->ts_first);
	item = proto_tree_add_time(tree, hf_tcp_ts_relative, tvb, 0, 0, &ts);
	PROTO_ITEM_SET_GENERATED(item);

	if( !tcppd )
		tcppd = p_get_proto_data(pinfo->fd, proto_tcp);

	if( tcppd ) {
		item = proto_tree_add_time(tree, hf_tcp_ts_delta, tvb, 0, 0,
			&tcppd->ts_del);
		PROTO_ITEM_SET_GENERATED(item);
	}
}

static void
print_pdu_tracking_data(packet_info *pinfo, tvbuff_t *tvb, proto_tree *tcp_tree, struct tcp_multisegment_pdu *msp)
{
	proto_item *item;

	if (check_col(pinfo->cinfo, COL_INFO)){
		col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[Continuation to #%u] ", msp->first_frame);
	}
	item=proto_tree_add_uint(tcp_tree, hf_tcp_continuation_to,
		tvb, 0, 0, msp->first_frame);
	PROTO_ITEM_SET_GENERATED(item);
}

/* if we know that a PDU starts inside this segment, return the adjusted
   offset to where that PDU starts or just return offset back
   and let TCP try to find out what it can about this segment
*/
static int
scan_for_next_pdu(tvbuff_t *tvb, proto_tree *tcp_tree, packet_info *pinfo, int offset, guint32 seq, guint32 nxtseq, emem_tree_t *multisegment_pdus)
{
        struct tcp_multisegment_pdu *msp=NULL;

	if(!pinfo->fd->flags.visited){
		msp=se_tree_lookup32_le(multisegment_pdus, seq-1);
		if(msp){
			/* If this is a continuation of a PDU started in a
			 * previous segment we need to update the last_frame
			 * variables.
			*/
			if(seq>msp->seq && seq<msp->nxtpdu){
				msp->last_frame=pinfo->fd->num;
				msp->last_frame_time=pinfo->fd->abs_ts;
				print_pdu_tracking_data(pinfo, tvb, tcp_tree, msp);
			}

			/* If this segment is completely within a previous PDU
			 * then we just skip this packet
			 */
			if(seq>msp->seq && nxtseq<=msp->nxtpdu){
				return -1;
			}
			if(seq<msp->nxtpdu && nxtseq>msp->nxtpdu){
				offset+=msp->nxtpdu-seq;
				return offset;
			}

		}
	} else {
		/* First we try to find the start and transfer time for a PDU.
		 * We only print this for the very first segment of a PDU
		 * and only for PDUs spanning multiple segments.
		 * Se we look for if there was any multisegment PDU started
		 * just BEFORE the end of this segment. I.e. either inside this
		 * segment or in a previous segment.
		 * Since this might also match PDUs that are completely within
		 * this segment we also verify that the found PDU does span
		 * beyond the end of this segment.
		 */
		msp=se_tree_lookup32_le(multisegment_pdus, nxtseq-1);
		if(msp){
			if( (pinfo->fd->num==msp->first_frame)
			){
				proto_item *item;
			 	nstime_t ns;

				item=proto_tree_add_uint(tcp_tree, hf_tcp_pdu_last_frame, tvb, 0, 0, msp->last_frame);
				PROTO_ITEM_SET_GENERATED(item);

				nstime_delta(&ns, &msp->last_frame_time, &pinfo->fd->abs_ts);
				item = proto_tree_add_time(tcp_tree, hf_tcp_pdu_time,
						tvb, 0, 0, &ns);
				PROTO_ITEM_SET_GENERATED(item);
			}
		}

		/* Second we check if this segment is part of a PDU started
		 * prior to the segment (seq-1)
		 */
		msp=se_tree_lookup32_le(multisegment_pdus, seq-1);
		if(msp){
			/* If this segment is completely within a previous PDU
			 * then we just skip this packet
			 */
			if(seq>msp->seq && nxtseq<=msp->nxtpdu){
				print_pdu_tracking_data(pinfo, tvb, tcp_tree, msp);
				return -1;
			}

			if(seq<msp->nxtpdu && nxtseq>msp->nxtpdu){
				offset+=msp->nxtpdu-seq;
				return offset;
			}
		}

	}
	return offset;
}

/* if we saw a PDU that extended beyond the end of the segment,
   use this function to remember where the next pdu starts
*/
struct tcp_multisegment_pdu *
pdu_store_sequencenumber_of_next_pdu(packet_info *pinfo, guint32 seq, guint32 nxtpdu, emem_tree_t *multisegment_pdus)
{
	struct tcp_multisegment_pdu *msp;

	msp=se_alloc(sizeof(struct tcp_multisegment_pdu));
	msp->nxtpdu=nxtpdu;
	msp->seq=seq;
	msp->first_frame=pinfo->fd->num;
	msp->last_frame=pinfo->fd->num;
	msp->last_frame_time=pinfo->fd->abs_ts;
	msp->flags=0;
	se_tree_insert32(multisegment_pdus, seq, (void *)msp);
	return msp;
}

/* This is called for SYN+ACK packets and the purpose is to verify that we
 * have seen window scaling in both directions.
 * If we cant find window scaling being set in both directions
 * that means it was present in the SYN but not in the SYN+ACK
 * (or the SYN was missing) and then we disable the window scaling
 * for this tcp session.
 */
static void
verify_tcp_window_scaling(struct tcp_analysis *tcpd)
{
	if( tcpd && ((tcpd->flow1.win_scale==-1) || (tcpd->flow2.win_scale==-1)) ){
		tcpd->flow1.win_scale=-1;
		tcpd->flow2.win_scale=-1;
	}
}

/* if we saw a window scaling option, store it for future reference
*/
static void
pdu_store_window_scale_option(guint8 ws, struct tcp_analysis *tcpd)
{
	if (tcpd)
		tcpd->fwd->win_scale=ws;
}

static void
tcp_get_relative_seq_ack(guint32 *seq, guint32 *ack, guint32 *win, struct tcp_analysis *tcpd)
{
	if (tcpd && tcp_relative_seq) {
		(*seq) -= tcpd->fwd->base_seq;
		(*ack) -= tcpd->rev->base_seq;
		if(tcpd->fwd->win_scale!=-1){
			(*win)<<=tcpd->fwd->win_scale;
		}
	}
}


/* when this function returns, it will (if createflag) populate the ta pointer.
 */
static void
tcp_analyze_get_acked_struct(guint32 frame, gboolean createflag, struct tcp_analysis *tcpd)
{
	if (!tcpd)
		return;

	tcpd->ta=se_tree_lookup32(tcpd->acked_table, frame);
	if((!tcpd->ta) && createflag){
		tcpd->ta=se_alloc0(sizeof(struct tcp_acked));
		se_tree_insert32(tcpd->acked_table, frame, (void *)tcpd->ta);
	}
}


/* fwd contains a list of all segments processed but not yet ACKed in the
 *     same direction as the current segment.
 * rev contains a list of all segments received but not yet ACKed in the
 *     opposite direction to the current segment.
 *
 * New segments are always added to the head of the fwd/rev lists.
 *
 */
static void
tcp_analyze_sequence_number(packet_info *pinfo, guint32 seq, guint32 ack, guint32 seglen, guint8 flags, guint32 window, struct tcp_analysis *tcpd)
{
	tcp_unacked_t *ual=NULL;
	int ackcount;

#ifdef REMOVED
printf("analyze_sequence numbers   frame:%u  direction:%s\n",pinfo->fd->num,direction>=0?"FWD":"REW");
printf("FWD list lastflags:0x%04x base_seq:0x%08x:\n",tcpd->fwd->lastsegmentflags,tcpd->fwd->base_seq);for(ual=tcpd->fwd->segments;ual;ual=ual->next)printf("Frame:%d Seq:%d Nextseq:%d\n",ual->frame,ual->seq,ual->nextseq);
printf("REV list lastflags:0x%04x base_seq:0x%08x:\n",tcpd->rev->lastsegmentflags,tcpd->rev->base_seq);for(ual=tcpd->rev->segments;ual;ual=ual->next)printf("Frame:%d Seq:%d Nextseq:%d\n",ual->frame,ual->seq,ual->nextseq);
#endif

	if (!tcpd) {
		return;
	}

	/* if this is the first segment for this list we need to store the
	 * base_seq
	 *
	 * Start relative seq and ack numbers at 1 if this
	 * is not a SYN packet. This makes the relative
	 * seq/ack numbers to be displayed correctly in the
	 * event that the SYN or SYN/ACK packet is not seen
	 * (this solves bug 1542)
	 */
	if(tcpd->fwd->base_seq==0){
		tcpd->fwd->base_seq = (flags & TH_SYN) ? seq : seq-1;
	}

	/* Only store reverse sequence if this isn't the SYN
  	 * There's no guarantee that the ACK field of a SYN
  	 * contains zeros; get the ISN from the first segment
	 * with the ACK bit set instead (usually the SYN/ACK).
	 */
	if( (tcpd->rev->base_seq==0) && (flags & TH_ACK) ){
		tcpd->rev->base_seq = (flags & TH_SYN) ? ack : ack-1;
	}


	/* ZERO WINDOW PROBE
	 * it is a zero window probe if
	 *  the sequnece number is the next expected one
	 *  the window in the other direction is 0
	 *  the segment is exactly 1 byte
	 */
/*QQQ tested*/
	if( seglen==1
	&&  seq==tcpd->fwd->nextseq
	&&  tcpd->rev->window==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_ZERO_WINDOW_PROBE;
		goto finished_fwd;
	}


	/* ZERO WINDOW
	 * a zero window packet has window == 0   but none of the SYN/FIN/RST set
	 */
/*QQQ tested*/
	if( window==0
	&& (flags&(TH_RST|TH_FIN|TH_SYN))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_ZERO_WINDOW;
	}


	/* LOST PACKET
	 * If this segment is beyond the last seen nextseq we must
	 * have missed some previous segment
	 *
	 * We only check for this if we have actually seen segments prior to this
	 * one.
	 * RST packets are not checked for this.
	 */
	if( tcpd->fwd->nextseq
	&&  GT_SEQ(seq, tcpd->fwd->nextseq)
	&&  (flags&(TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_LOST_PACKET;
	}


	/* KEEP ALIVE
	 * a keepalive contains 0 or 1 bytes of data and starts one byte prior
	 * to what should be the next sequence number.
	 * SYN/FIN/RST segments are never keepalives
	 */
/*QQQ tested */
	if( (seglen==0||seglen==1)
	&&  seq==(tcpd->fwd->nextseq-1)
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_KEEP_ALIVE;
	}

	/* WINDOW UPDATE
	 * A window update is a 0 byte segment with the same SEQ/ACK numbers as
	 * the previous seen segment and with a new window value
	 */
	if( seglen==0
	&&  window
	&&  window!=tcpd->fwd->window
	&&  seq==tcpd->fwd->nextseq
	&&  ack==tcpd->fwd->lastack
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_WINDOW_UPDATE;
	}


	/* WINDOW FULL
	 * If we know the window scaling
	 * and if this segment contains data ang goes all the way to the
	 * edge of the advertized window
	 * then we mark it as WINDOW FULL
	 * SYN/RST/FIN packets are never WINDOW FULL
	 */
/*QQQ tested*/
	if( seglen>0
	&&  tcpd->fwd->win_scale!=-1
	&&  tcpd->rev->win_scale!=-1
	&&  (seq+seglen)==(tcpd->rev->lastack+(tcpd->rev->window<<tcpd->rev->win_scale))
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_WINDOW_FULL;
	}


	/* KEEP ALIVE ACK
	 * It is a keepalive ack if it repeats the previous ACK and if
	 * the last segment in the reverse direction was a keepalive
	 */
/*QQQ tested*/
	if( seglen==0
	&&  window
	&&  window==tcpd->fwd->window
	&&  seq==tcpd->fwd->nextseq
	&&  ack==tcpd->fwd->lastack
	&& (tcpd->rev->lastsegmentflags&TCP_A_KEEP_ALIVE)
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_KEEP_ALIVE_ACK;
		goto finished_fwd;
	}


	/* ZERO WINDOW PROBE ACK
	 * It is a zerowindowprobe ack if it repeats the previous ACK and if
	 * the last segment in the reverse direction was a zerowindowprobe
	 * It also repeats the previous zero window indication
	 */
/*QQQ tested*/
	if( seglen==0
	&&  window==0
	&&  window==tcpd->fwd->window
	&&  seq==tcpd->fwd->nextseq
	&&  ack==tcpd->fwd->lastack
	&& (tcpd->rev->lastsegmentflags&TCP_A_ZERO_WINDOW_PROBE)
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_ZERO_WINDOW_PROBE_ACK;
		goto finished_fwd;
	}


	/* DUPLICATE ACK
	 * It is a duplicate ack if window/seq/ack is the same as the previous
	 * segment and if the segment length is 0
	 */
	if( seglen==0
	&&  window
	&&  window==tcpd->fwd->window
	&&  seq==tcpd->fwd->nextseq
	&&  ack==tcpd->fwd->lastack
	&&  (flags&(TH_SYN|TH_FIN|TH_RST))==0 ){
		tcpd->fwd->dupacknum++;
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_DUPLICATE_ACK;
		tcpd->ta->dupack_num=tcpd->fwd->dupacknum;
		tcpd->ta->dupack_frame=tcpd->fwd->lastnondupack;
	}



finished_fwd:
	/* If this was NOT a dupack we must reset the dupack counters */
	if( (!tcpd->ta) || !(tcpd->ta->flags&TCP_A_DUPLICATE_ACK) ){
		tcpd->fwd->lastnondupack=pinfo->fd->num;
		tcpd->fwd->dupacknum=0;
	}


	/* ACKED LOST PACKET
	 * If this segment acks beyond the nextseqnum in the other direction
	 * then that means we have missed packets going in the
	 * other direction
	 *
	 * We only check this if we have actually seen some seq numbers
	 * in the other direction.
	 */
	if( tcpd->rev->nextseq
	&&  GT_SEQ(ack, tcpd->rev->nextseq )
	&&  (flags&(TH_ACK))!=0 ){
/*QQQ tested*/
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_ACK_LOST_PACKET;
		/* update nextseq in the other direction so we dont get
		 * this indication again.
		 */
		tcpd->rev->nextseq=ack;
	}


	/* RETRANSMISSION/FAST RETRANSMISSION/OUT-OF-ORDER
	 * If the segments contains data and if it does not advance
	 * sequence number it must be either of these three.
	 * Only test for this if we know what the seq number should be
	 * (tcpd->fwd->nextseq)
	 *
	 * Note that a simple KeepAlive is not a retransmission
	 */
	if( seglen>0
	&&  tcpd->fwd->nextseq
	&&  (LT_SEQ(seq, tcpd->fwd->nextseq)) ){
		guint64 t;

		if(tcpd->ta && (tcpd->ta->flags&TCP_A_KEEP_ALIVE) ){
			goto finished_checking_retransmission_type;
		}

		/* If there were >=2 duplicate ACKs in the reverse direction
		 * (there might be duplicate acks missing from the trace)
		 * and if this sequence number matches those ACKs
		 * and if the packet occurs within 20ms of the last
		 * duplicate ack
		 * then this is a fast retransmission
		 */
		t=(pinfo->fd->abs_ts.secs-tcpd->rev->lastacktime.secs)*1000000000;
		t=t+(pinfo->fd->abs_ts.nsecs)-tcpd->rev->lastacktime.nsecs;
		if( tcpd->rev->dupacknum>=2
		&&  tcpd->rev->lastack==seq
		&&  t<20000000 ){
			if(!tcpd->ta){
				tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
			}
			tcpd->ta->flags|=TCP_A_FAST_RETRANSMISSION;
			goto finished_checking_retransmission_type;
		}

		/* If the segment came <3ms since the segment with the highest
		 * seen sequence number, then it is an OUT-OF-ORDER segment.
		 *   (3ms is an arbitrary number)
		 */
		t=(pinfo->fd->abs_ts.secs-tcpd->fwd->nextseqtime.secs)*1000000000;
		t=t+(pinfo->fd->abs_ts.nsecs)-tcpd->fwd->nextseqtime.nsecs;
		if( t<3000000 ){
			if(!tcpd->ta){
				tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
			}
			tcpd->ta->flags|=TCP_A_OUT_OF_ORDER;
			goto finished_checking_retransmission_type;
		}

		/* Then it has to be a generic retransmission */
		if(!tcpd->ta){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
		}
		tcpd->ta->flags|=TCP_A_RETRANSMISSION;
		nstime_delta(&tcpd->ta->rto_ts, &pinfo->fd->abs_ts, &tcpd->fwd->nextseqtime);
		tcpd->ta->rto_frame=tcpd->fwd->nextseqframe;
	}
finished_checking_retransmission_type:


	/* add this new sequence number to the fwd list */
	TCP_UNACKED_NEW(ual);
	ual->next=tcpd->fwd->segments;
	tcpd->fwd->segments=ual;
	ual->frame=pinfo->fd->num;
	ual->seq=seq;
	ual->ts=pinfo->fd->abs_ts;

	/* next sequence number is seglen bytes away, plus SYN/FIN which counts as one byte */
	ual->nextseq=seq+seglen;
	if( flags&(TH_SYN|TH_FIN) ){
		ual->nextseq+=1;
	}

	/* Store the highest number seen so far for nextseq so we can detect
	 * when we receive segments that arrive with a "hole"
	 * If we dont have anything since before, just store what we got.
	 * ZeroWindowProbes are special and dont really advance the nextseq
	 */
	if(GT_SEQ(ual->nextseq, tcpd->fwd->nextseq) || !tcpd->fwd->nextseq) {
		if( !tcpd->ta || !(tcpd->ta->flags&TCP_A_ZERO_WINDOW_PROBE) ){
			tcpd->fwd->nextseq=ual->nextseq;
			tcpd->fwd->nextseqframe=pinfo->fd->num;
			tcpd->fwd->nextseqtime.secs=pinfo->fd->abs_ts.secs;
			tcpd->fwd->nextseqtime.nsecs=pinfo->fd->abs_ts.nsecs;
		}
	}


	/* remember what the ack/window is so we can track window updates and retransmissions */
	tcpd->fwd->window=window;
	tcpd->fwd->lastack=ack;
	tcpd->fwd->lastacktime.secs=pinfo->fd->abs_ts.secs;
	tcpd->fwd->lastacktime.nsecs=pinfo->fd->abs_ts.nsecs;


	/* if there were any flags set for this segment we need to remember them
	 * we only remember the flags for the very last segment though.
	 */
	if(tcpd->ta){
		tcpd->fwd->lastsegmentflags=tcpd->ta->flags;
	} else {
		tcpd->fwd->lastsegmentflags=0;
	}


	/* remove all segments this ACKs and we dont need to keep around any more
	 */
	ackcount=0;
	/* first we remove all such segments at the head of the list */
	while((ual=tcpd->rev->segments)){
		tcp_unacked_t *tmpual;
		if(ack==ual->nextseq){
			tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
			tcpd->ta->frame_acked=ual->frame;
			nstime_delta(&tcpd->ta->ts, &pinfo->fd->abs_ts, &ual->ts);
		}
		if(GT_SEQ(ual->nextseq,ack)){
			break;
		}
		if(!ackcount){
/*qqq do the ACKs segment x  delta y */
		}
		ackcount++;
		tmpual=tcpd->rev->segments->next;

		if (tcpd->rev->scps_capable) {
		  /* Track largest segment successfully sent for SNACK analysis */
		  if ((ual->nextseq - ual->seq) > tcpd->fwd->maxsizeacked) {
		    tcpd->fwd->maxsizeacked = (ual->nextseq - ual->seq);
		  }
		}

		TCP_UNACKED_FREE(ual);
		tcpd->rev->segments=tmpual;
	}
	/* now we remove all such segments that are NOT at the head of the list */
	ual=tcpd->rev->segments;
	while(ual && ual->next){
		tcp_unacked_t *tmpual;
		if(GT_SEQ(ual->next->nextseq,ack)){
			ual=ual->next;
			continue;
		}
		if(!ackcount){
/*qqq do the ACKs segment x  delta y */
		}
		ackcount++;
		tmpual=ual->next->next;

		if (tcpd->rev->scps_capable) {
		  /* Track largest segment successfully sent for SNACK analysis*/
		  if ((ual->next->nextseq - ual->next->seq) > tcpd->fwd->maxsizeacked){
		    tcpd->fwd->maxsizeacked = (ual->next->nextseq - ual->next->seq);
		  }
		}

		TCP_UNACKED_FREE(ual->next);
		ual->next=tmpual;
		ual=ual->next;
	}

	/* how many bytes of data are there in flight after this frame
	 * was sent
	 */
	ual=tcpd->fwd->segments;
	if (tcp_track_bytes_in_flight && seglen!=0 && ual) {
		guint32 first_seq, last_seq, in_flight;

		first_seq = ual->seq - tcpd->fwd->base_seq;
		last_seq = ual->nextseq - tcpd->fwd->base_seq;
		while (ual) {
			if ((ual->nextseq-tcpd->fwd->base_seq)>last_seq) {
				last_seq = ual->nextseq-tcpd->fwd->base_seq;
			}
			if ((ual->seq-tcpd->fwd->base_seq)<first_seq) {
				first_seq = ual->seq-tcpd->fwd->base_seq;
			}
			ual = ual->next;
		}
		in_flight = last_seq-first_seq;

		if (in_flight>0 && in_flight<2000000000) {
			if(!tcpd->ta){
				tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
			}
			tcpd->ta->bytes_in_flight = in_flight;
		}
	}

}

/*
 * Prints results of the sequence number analysis concerning tcp segments
 * retransmitted or out-of-order
 */
static void
tcp_sequence_number_analysis_print_retransmission(packet_info * pinfo,
						  tvbuff_t * tvb,
						  proto_tree * flags_tree,
						  struct tcp_acked *ta
						  )
{
  proto_item * flags_item;

  /* TCP Rentransmission */
  if (ta->flags & TCP_A_RETRANSMISSION) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_retransmission,
					  tvb, 0, 0,
					  "This frame is a (suspected) "
					  "retransmission"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					  "Retransmission (suspected)");

    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Retransmission] ");
    }
    if (ta->rto_ts.secs || ta->rto_ts.nsecs) {
      flags_item = proto_tree_add_time(flags_tree, hf_tcp_analysis_rto,
					      tvb, 0, 0, &ta->rto_ts);
      PROTO_ITEM_SET_GENERATED(flags_item);
      flags_item=proto_tree_add_uint(flags_tree, hf_tcp_analysis_rto_frame,
					      tvb, 0, 0, ta->rto_frame);
      PROTO_ITEM_SET_GENERATED(flags_item);
    }
  }
  /* TCP Fast Rentransmission */
  if (ta->flags & TCP_A_FAST_RETRANSMISSION) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_fast_retransmission,
					  tvb, 0, 0,
					  "This frame is a (suspected) fast"
					  " retransmission"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
					  "Fast retransmission (suspected)");
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_retransmission,
					  tvb, 0, 0,
					  "This frame is a (suspected) "
					  "retransmission"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					  "[TCP Fast Retransmission] ");
    }
  }
  /* TCP Out-Of-Order */
  if (ta->flags & TCP_A_OUT_OF_ORDER) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_out_of_order,
					  tvb, 0, 0,
					  "This frame is a (suspected) "
					  "out-of-order segment"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
					  "Out-Of-Order segment");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Out-Of-Order] ");
    }
  }
}

/* Prints results of the sequence number analysis concerning reused ports */
static void
tcp_sequence_number_analysis_print_reused(packet_info * pinfo,
					  tvbuff_t * tvb,
					  proto_tree * flags_tree,
					  struct tcp_acked *ta
					  )
{
  proto_item * flags_item;

  /* TCP Ports Reused */
  if (ta->flags & TCP_A_REUSED_PORTS) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_reused_ports,
					  tvb, 0, 0,
					  "A new tcp session is started with the same "
					  "ports as an earlier session in this trace"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
			    "TCP Port numbers reused for new session");
    if(check_col(pinfo->cinfo, COL_INFO)){
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					    "[TCP Port numbers reused] ");
    }
  }
}

/* Prints results of the sequence number analysis concerning lost tcp segments */
static void
tcp_sequence_number_analysis_print_lost(packet_info * pinfo,
					tvbuff_t * tvb,
					proto_tree * flags_tree,
					struct tcp_acked *ta
					)
{
  proto_item * flags_item;

  /* TCP Lost Segment */
  if (ta->flags & TCP_A_LOST_PACKET) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_lost_packet,
					  tvb, 0, 0,
					  "A segment before this frame was "
					  "lost"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
			    "Previous segment lost (common at capture start)");
    if(check_col(pinfo->cinfo, COL_INFO)){
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					    "[TCP Previous segment lost] ");
    }
  }
  /* TCP Ack lost segment */
  if (ta->flags & TCP_A_ACK_LOST_PACKET) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_ack_lost_packet,
					  tvb, 0, 0,
					  "This frame ACKs a segment we have "
					  "not seen (lost?)"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
			      "ACKed lost segment (common at capture start)");
    if(check_col(pinfo->cinfo, COL_INFO)){
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					  "[TCP ACKed lost segment] ");
    }
  }
}

/* Prints results of the sequence number analysis concerning tcp window */
static void
tcp_sequence_number_analysis_print_window(packet_info * pinfo,
					  tvbuff_t * tvb,
					  proto_tree * flags_tree,
					  struct tcp_acked *ta
					  )
{
  proto_item * flags_item;

  /* TCP Window Update */
  if (ta->flags & TCP_A_WINDOW_UPDATE) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_window_update,
					  tvb, 0, 0,
					  "This is a tcp window update"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_CHAT,
					  "Window update");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Window Update] ");
    }
  }
  /* TCP Full Window */
  if (ta->flags & TCP_A_WINDOW_FULL) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_window_full,
					  tvb, 0, 0,
					  "The transmission window is now "
					  "completely full"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
					  "Window is full");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Window Full] ");
    }
  }
}

/* Prints results of the sequence number analysis concerning tcp keepalive */
static void
tcp_sequence_number_analysis_print_keepalive(packet_info * pinfo,
					  tvbuff_t * tvb,
					  proto_tree * flags_tree,
					  struct tcp_acked *ta
					  )
{
  proto_item * flags_item;

  /*TCP Keep Alive */
  if (ta->flags & TCP_A_KEEP_ALIVE){
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_keep_alive,
					  tvb, 0, 0,
					  "This is a TCP keep-alive segment"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					  "Keep-Alive");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Keep-Alive] ");
    }
  }
  /* TCP Ack Keep Alive */
  if (ta->flags & TCP_A_KEEP_ALIVE_ACK) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_keep_alive_ack,
					  tvb, 0, 0,
					  "This is an ACK to a TCP keep-alive "
					  "segment"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					  "Keep-Alive ACK");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP Keep-Alive ACK] ");
    }
  }
}

/* Prints results of the sequence number analysis concerning tcp duplicate ack */
static void
tcp_sequence_number_analysis_print_duplicate(packet_info * pinfo,
					      tvbuff_t * tvb,
					      proto_tree * flags_tree,
					      struct tcp_acked *ta,
					      proto_tree * tree
					    )
{
  proto_item * flags_item;

  /* TCP Duplicate ACK */
  if (ta->dupack_num) {
    if (ta->flags & TCP_A_DUPLICATE_ACK ) {
      flags_item=proto_tree_add_none_format(flags_tree,
					    hf_tcp_analysis_duplicate_ack,
					    tvb, 0, 0,
					    "This is a TCP duplicate ack"
					    );
      PROTO_ITEM_SET_GENERATED(flags_item);
      if (check_col(pinfo->cinfo, COL_INFO)) {
	col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					    "[TCP Dup ACK %u#%u] ",
					    ta->dupack_frame,
					    ta->dupack_num
					    );
      }
    }
    flags_item=proto_tree_add_uint(tree, hf_tcp_analysis_duplicate_ack_num,
					    tvb, 0, 0, ta->dupack_num);
    PROTO_ITEM_SET_GENERATED(flags_item);
    flags_item=proto_tree_add_uint(tree, hf_tcp_analysis_duplicate_ack_frame,
					    tvb, 0, 0, ta->dupack_frame);
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					    "Duplicate ACK (#%u)",
					    ta->dupack_num
					    );
  }
}

/* Prints results of the sequence number analysis concerning tcp zero window */
static void
tcp_sequence_number_analysis_print_zero_window(packet_info * pinfo,
					      tvbuff_t * tvb,
					      proto_tree * flags_tree,
					      struct tcp_acked *ta
					    )
{
  proto_item * flags_item;

  /* TCP Zero Window Probe */
  if (ta->flags & TCP_A_ZERO_WINDOW_PROBE) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_zero_window_probe,
					  tvb, 0, 0,
					  "This is a TCP zero-window-probe"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					  "Zero window probe");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP ZeroWindowProbe] ");
    }
  }
  /* TCP Zero Window */
  if (ta->flags&TCP_A_ZERO_WINDOW) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_zero_window,
					  tvb, 0, 0,
					  "This is a ZeroWindow segment"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_WARN,
					  "Zero window");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO, "[TCP ZeroWindow] ");
    }
  }
  /* TCP Zero Window Probe Ack */
  if (ta->flags & TCP_A_ZERO_WINDOW_PROBE_ACK) {
    flags_item=proto_tree_add_none_format(flags_tree,
					  hf_tcp_analysis_zero_window_probe_ack,
					  tvb, 0, 0,
					  "This is an ACK to a TCP zero-window-probe"
					  );
    PROTO_ITEM_SET_GENERATED(flags_item);
    expert_add_info_format(pinfo, flags_item, PI_SEQUENCE, PI_NOTE,
					  "Zero window probe ACK");
    if (check_col(pinfo->cinfo, COL_INFO)) {
      col_prepend_fence_fstr(pinfo->cinfo, COL_INFO,
					  "[TCP ZeroWindowProbeAck] ");
    }
  }
}


/* Prints results of the sequence number analysis concerning how many bytes of data are in flight */
static void
tcp_sequence_number_analysis_print_bytes_in_flight(packet_info * pinfo _U_,
					      tvbuff_t * tvb _U_,
					      proto_tree * flags_tree _U_,
					      struct tcp_acked *ta
					    )
{
  proto_item * flags_item;

  if (tcp_track_bytes_in_flight) {
    flags_item=proto_tree_add_uint(flags_tree,
				hf_tcp_analysis_bytes_in_flight,
				tvb, 0, 0, ta->bytes_in_flight);

    PROTO_ITEM_SET_GENERATED(flags_item);
  }
}

static void
tcp_print_sequence_number_analysis(packet_info *pinfo, tvbuff_t *tvb, proto_tree *parent_tree, struct tcp_analysis *tcpd)
{
	struct tcp_acked *ta = NULL;
	proto_item *item;
	proto_tree *tree;
	proto_tree *flags_tree=NULL;

	if (!tcpd) {
		return;
	}
	if(!tcpd->ta){
		tcp_analyze_get_acked_struct(pinfo->fd->num, FALSE, tcpd);
	}
	ta=tcpd->ta;
	if(!ta){
		return;
	}

	item=proto_tree_add_text(parent_tree, tvb, 0, 0, "SEQ/ACK analysis");
	PROTO_ITEM_SET_GENERATED(item);
	tree=proto_item_add_subtree(item, ett_tcp_analysis);

	/* encapsulate all proto_tree_add_xxx in ifs so we only print what
	   data we actually have */
	if(ta->frame_acked){
		item = proto_tree_add_uint(tree, hf_tcp_analysis_acks_frame,
			tvb, 0, 0, ta->frame_acked);
        	PROTO_ITEM_SET_GENERATED(item);

		/* only display RTT if we actually have something we are acking */
		if( ta->ts.secs || ta->ts.nsecs ){
			item = proto_tree_add_time(tree, hf_tcp_analysis_ack_rtt,
			tvb, 0, 0, &ta->ts);
        		PROTO_ITEM_SET_GENERATED(item);
		}
	}

	if(ta->bytes_in_flight) {
		/* print results for amount of data in flight */
		tcp_sequence_number_analysis_print_bytes_in_flight(pinfo, tvb, tree, ta);
	}

	if(ta->flags){
		item = proto_tree_add_item(tree, hf_tcp_analysis_flags, tvb, 0, 0, FALSE);
		PROTO_ITEM_SET_GENERATED(item);
		flags_tree=proto_item_add_subtree(item, ett_tcp_analysis);

		/* print results for reused tcp ports */
		tcp_sequence_number_analysis_print_reused(pinfo, tvb, flags_tree, ta);

		/* print results for retransmission and out-of-order segments */
		tcp_sequence_number_analysis_print_retransmission(pinfo, tvb, flags_tree, ta);

		/* print results for lost tcp segments */
		tcp_sequence_number_analysis_print_lost(pinfo, tvb, flags_tree, ta);

		/* print results for tcp window information */
		tcp_sequence_number_analysis_print_window(pinfo, tvb, flags_tree, ta);

		/* print results for tcp keep alive information */
		tcp_sequence_number_analysis_print_keepalive(pinfo, tvb, flags_tree, ta);

		/* print results for tcp duplicate acks */
		tcp_sequence_number_analysis_print_duplicate(pinfo, tvb, flags_tree, ta, tree);

		/* print results for tcp zero window  */
		tcp_sequence_number_analysis_print_zero_window(pinfo, tvb, flags_tree, ta);

	}

}

static void
print_tcp_fragment_tree(fragment_data *ipfd_head, proto_tree *tree, proto_tree *tcp_tree, packet_info *pinfo, tvbuff_t *next_tvb)
{
	proto_item *tcp_tree_item, *frag_tree_item;

	/*
	 * The subdissector thought it was completely
	 * desegmented (although the stuff at the
	 * end may, in turn, require desegmentation),
	 * so we show a tree with all segments.
	 */
	show_fragment_tree(ipfd_head, &tcp_segment_items,
		tree, pinfo, next_tvb, &frag_tree_item);
	/*
	 * The toplevel fragment subtree is now
	 * behind all desegmented data; move it
	 * right behind the TCP tree.
	 */
	tcp_tree_item = proto_tree_get_parent(tcp_tree);
	if(frag_tree_item && tcp_tree_item) {
		proto_tree_move_item(tree, tcp_tree_item, frag_tree_item);
	}
}

/* **************************************************************************
 * End of tcp sequence number analysis
 * **************************************************************************/


/* Minimum TCP header length. */
#define TCPH_MIN_LEN            20

/*
 *	TCP option
 */

#define TCPOPT_NOP              1       /* Padding */
#define TCPOPT_EOL              0       /* End of options */
#define TCPOPT_MSS              2       /* Segment size negotiating */
#define TCPOPT_WINDOW           3       /* Window scaling */
#define TCPOPT_SACK_PERM        4       /* SACK Permitted */
#define TCPOPT_SACK             5       /* SACK Block */
#define TCPOPT_ECHO             6
#define TCPOPT_ECHOREPLY        7
#define TCPOPT_TIMESTAMP        8       /* Better RTT estimations/PAWS */
#define TCPOPT_CC               11
#define TCPOPT_CCNEW            12
#define TCPOPT_CCECHO           13
#define TCPOPT_MD5              19      /* RFC2385 */
#define TCPOPT_SCPS             20      /* SCPS Capabilities */
#define TCPOPT_SNACK            21      /* SCPS SNACK */
#define TCPOPT_RECBOUND         22      /* SCPS Record Boundary */
#define TCPOPT_CORREXP          23      /* SCPS Corruption Experienced */
#define TCPOPT_QS               27      /* RFC4782 */

/*
 *     TCP option lengths
 */

#define TCPOLEN_MSS            4
#define TCPOLEN_WINDOW         3
#define TCPOLEN_SACK_PERM      2
#define TCPOLEN_SACK_MIN       2
#define TCPOLEN_ECHO           6
#define TCPOLEN_ECHOREPLY      6
#define TCPOLEN_TIMESTAMP      10
#define TCPOLEN_CC             6
#define TCPOLEN_CCNEW          6
#define TCPOLEN_CCECHO         6
#define TCPOLEN_MD5            18
#define TCPOLEN_SCPS           4
#define TCPOLEN_SNACK          6
#define TCPOLEN_RECBOUND       2
#define TCPOLEN_CORREXP        2
#define TCPOLEN_QS             8



/* Desegmentation of TCP streams */
/* table to hold defragmented TCP streams */
static GHashTable *tcp_fragment_table = NULL;
static void
tcp_fragment_init(void)
{
	fragment_table_init(&tcp_fragment_table);
}

/* functions to trace tcp segments */
/* Enable desegmenting of TCP streams */
static gboolean tcp_desegment = TRUE;

static void
desegment_tcp(tvbuff_t *tvb, packet_info *pinfo, int offset,
		guint32 seq, guint32 nxtseq,
		guint32 sport, guint32 dport,
		proto_tree *tree, proto_tree *tcp_tree,
		struct tcp_analysis *tcpd)
{
	struct tcpinfo *tcpinfo = pinfo->private_data;
	fragment_data *ipfd_head;
	int last_fragment_len;
	gboolean must_desegment;
	gboolean called_dissector;
	int another_pdu_follows;
	int deseg_offset;
	guint32 deseg_seq;
	gint nbytes;
	proto_item *item;
	struct tcp_multisegment_pdu *msp;
	gboolean cleared_writable = col_get_writable(pinfo->cinfo);

again:
	ipfd_head=NULL;
	last_fragment_len=0;
	must_desegment = FALSE;
	called_dissector = FALSE;
	another_pdu_follows = 0;
	msp=NULL;

	/*
	 * Initialize these to assume no desegmentation.
	 * If that's not the case, these will be set appropriately
	 * by the subdissector.
	 */
	pinfo->desegment_offset = 0;
	pinfo->desegment_len = 0;

	/*
	 * Initialize this to assume that this segment will just be
	 * added to the middle of a desegmented chunk of data, so
	 * that we should show it all as data.
	 * If that's not the case, it will be set appropriately.
	 */
	deseg_offset = offset;

	/* find the most previous PDU starting before this sequence number */
	if (tcpd) {
		msp = se_tree_lookup32_le(tcpd->fwd->multisegment_pdus, seq-1);
	}
	if(msp && msp->seq<=seq && msp->nxtpdu>seq){
		int len;

		if(!pinfo->fd->flags.visited){
			msp->last_frame=pinfo->fd->num;
			msp->last_frame_time=pinfo->fd->abs_ts;
		}

		/* OK, this PDU was found, which means the segment continues
		   a higher-level PDU and that we must desegment it.
		*/
		if(msp->flags&MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT){
			/* The dissector asked for the entire segment */
			len=tvb_length_remaining(tvb, offset);
		} else {
			len=MIN(nxtseq, msp->nxtpdu) - seq;
		}
		last_fragment_len = len;

		ipfd_head = fragment_add(tvb, offset, pinfo, msp->first_frame,
			tcp_fragment_table,
			seq - msp->seq,
			len,
			(LT_SEQ (nxtseq,msp->nxtpdu)) );

		if(msp->flags&MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT){
			msp->flags&=(~MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT);

			/* If we consumed the entire segment there is no
			 * other pdu starting anywhere inside this segment.
			 * So update nxtpdu to point at least to the start
			 * of the next segment.
			 * (If the subdissector asks for even more data we
			 * will advance nxtpdu even furhter later down in
			 * the code.)
			 */
			msp->nxtpdu=nxtseq;
		}

		if( (msp->nxtpdu<nxtseq)
		&&  (msp->nxtpdu>=seq)
		&&  (len>0) ){
			another_pdu_follows=msp->nxtpdu-seq;
		}
	} else {
		/* This segment was not found in our table, so it doesn't
		   contain a continuation of a higher-level PDU.
		   Call the normal subdissector.
		*/
		process_tcp_payload(tvb, offset, pinfo, tree, tcp_tree,
				sport, dport, 0, 0, FALSE, tcpd);
		called_dissector = TRUE;

		/* Did the subdissector ask us to desegment some more data
		   before it could handle the packet?
		   If so we have to create some structures in our table but
		   this is something we only do the first time we see this
		   packet.
		*/
		if(pinfo->desegment_len) {
			if (!pinfo->fd->flags.visited)
				must_desegment = TRUE;

			/*
			 * Set "deseg_offset" to the offset in "tvb"
			 * of the first byte of data that the
			 * subdissector didn't process.
			 */
			deseg_offset = offset + pinfo->desegment_offset;
		}

		/* Either no desegmentation is necessary, or this is
		   segment contains the beginning but not the end of
		   a higher-level PDU and thus isn't completely
		   desegmented.
		*/
		ipfd_head = NULL;
	}


	/* is it completely desegmented? */
	if(ipfd_head){
		/*
		 * Yes, we think it is.
		 * We only call subdissector for the last segment.
		 * Note that the last segment may include more than what
		 * we needed.
		 */
		if(ipfd_head->reassembled_in==pinfo->fd->num){
			/*
			 * OK, this is the last segment.
			 * Let's call the subdissector with the desegmented
			 * data.
			 */
			tvbuff_t *next_tvb;
			int old_len;

			/* create a new TVB structure for desegmented data */
			next_tvb = tvb_new_child_real_data(tvb, ipfd_head->data,
					ipfd_head->datalen, ipfd_head->datalen);


			/* add desegmented data to the data source list */
			add_new_data_source(pinfo, next_tvb, "Reassembled TCP");

			/*
			 * Supply the sequence number of the first of the
			 * reassembled bytes.
			 */
			tcpinfo->seq = msp->seq;

			/* indicate that this is reassembled data */
			tcpinfo->is_reassembled = TRUE;

			/* call subdissector */
			process_tcp_payload(next_tvb, 0, pinfo, tree,
			    tcp_tree, sport, dport, 0, 0, FALSE, tcpd);
			called_dissector = TRUE;

			/*
			 * OK, did the subdissector think it was completely
			 * desegmented, or does it think we need even more
			 * data?
			 */
			old_len=(int)(tvb_reported_length(next_tvb)-last_fragment_len);
			if(pinfo->desegment_len &&
			    pinfo->desegment_offset<=old_len){
				/*
				 * "desegment_len" isn't 0, so it needs more
				 * data for something - and "desegment_offset"
				 * is before "old_len", so it needs more data
				 * to dissect the stuff we thought was
				 * completely desegmented (as opposed to the
				 * stuff at the beginning being completely
				 * desegmented, but the stuff at the end
				 * being a new higher-level PDU that also
				 * needs desegmentation).
				 */
				fragment_set_partial_reassembly(pinfo,msp->first_frame,tcp_fragment_table);
				/* Update msp->nxtpdu to point to the new next
				 * pdu boundary.
				 */
				if(pinfo->desegment_len==DESEGMENT_ONE_MORE_SEGMENT){
					/* We want reassembly of at least one
					 * more segment so set the nxtpdu
					 * boundary to one byte into the next
					 * segment.
					 * This means that the next segment
					 * will complete reassembly even if it
					 * is only one single byte in length.
					 */
					msp->nxtpdu=seq+tvb_reported_length_remaining(tvb, offset) + 1;
					msp->flags|=MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT;
				} else {
					msp->nxtpdu=seq + last_fragment_len + pinfo->desegment_len;
				}
				/* Since we need at least some more data
				 * there can be no pdu following in the
				 * tail of this segment.
				 */
				another_pdu_follows=0;
				offset += last_fragment_len;
				seq += last_fragment_len;
				if (tvb_length_remaining(tvb, offset) > 0)
					goto again;
			} else {
				/*
				 * Show the stuff in this TCP segment as
				 * just raw TCP segment data.
				 */
				nbytes = another_pdu_follows > 0
					? another_pdu_follows
					: tvb_reported_length_remaining(tvb, offset);
				proto_tree_add_text(tcp_tree, tvb, offset, nbytes,
				    "TCP segment data (%u byte%s)", nbytes,
				    plurality(nbytes, "", "s"));

				print_tcp_fragment_tree(ipfd_head, tree, tcp_tree, pinfo, next_tvb);

				/* Did the subdissector ask us to desegment
				   some more data?  This means that the data
				   at the beginning of this segment completed
				   a higher-level PDU, but the data at the
				   end of this segment started a higher-level
				   PDU but didn't complete it.

				   If so, we have to create some structures
				   in our table, but this is something we
				   only do the first time we see this packet.
				*/
				if(pinfo->desegment_len) {
					if (!pinfo->fd->flags.visited)
						must_desegment = TRUE;

					/* The stuff we couldn't dissect
					   must have come from this segment,
					   so it's all in "tvb".

				 	   "pinfo->desegment_offset" is
				 	   relative to the beginning of
				 	   "next_tvb"; we want an offset
				 	   relative to the beginning of "tvb".

				 	   First, compute the offset relative
				 	   to the *end* of "next_tvb" - i.e.,
				 	   the number of bytes before the end
				 	   of "next_tvb" at which the
				 	   subdissector stopped.  That's the
				 	   length of "next_tvb" minus the
				 	   offset, relative to the beginning
				 	   of "next_tvb, at which the
				 	   subdissector stopped.
				 	*/
					deseg_offset =
					    ipfd_head->datalen - pinfo->desegment_offset;

					/* "tvb" and "next_tvb" end at the
					   same byte of data, so the offset
					   relative to the end of "next_tvb"
					   of the byte at which we stopped
					   is also the offset relative to
					   the end of "tvb" of the byte at
					   which we stopped.

					   Convert that back into an offset
					   relative to the beginninng of
					   "tvb", by taking the length of
					   "tvb" and subtracting the offset
					   relative to the end.
					*/
					deseg_offset=tvb_reported_length(tvb) - deseg_offset;
				}
			}
		}
	}

	if (must_desegment) {
	    /* If the dissector requested "reassemble until FIN"
	     * just set this flag for the flow and let reassembly
	     * proceed at normal.  We will check/pick up these
	     * reassembled PDUs later down in dissect_tcp() when checking
	     * for the FIN flag.
	     */
	    if(tcpd && pinfo->desegment_len==DESEGMENT_UNTIL_FIN) {
		tcpd->fwd->flags|=TCP_FLOW_REASSEMBLE_UNTIL_FIN;
	    }
	    /*
	     * The sequence number at which the stuff to be desegmented
	     * starts is the sequence number of the byte at an offset
	     * of "deseg_offset" into "tvb".
	     *
	     * The sequence number of the byte at an offset of "offset"
	     * is "seq", i.e. the starting sequence number of this
	     * segment, so the sequence number of the byte at
	     * "deseg_offset" is "seq + (deseg_offset - offset)".
	     */
	    deseg_seq = seq + (deseg_offset - offset);

	    if(tcpd && ((nxtseq - deseg_seq) <= 1024*1024)
	    &&  (!pinfo->fd->flags.visited) ){
		if(pinfo->desegment_len==DESEGMENT_ONE_MORE_SEGMENT){
			/* The subdissector asked to reassemble using the
			 * entire next segment.
			 * Just ask reassembly for one more byte
			 * but set this msp flag so we can pick it up
			 * above.
			 */
			msp = pdu_store_sequencenumber_of_next_pdu(pinfo,
				deseg_seq, nxtseq+1, tcpd->fwd->multisegment_pdus);
			msp->flags|=MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT;
		} else {
			msp = pdu_store_sequencenumber_of_next_pdu(pinfo,
				deseg_seq, nxtseq+pinfo->desegment_len, tcpd->fwd->multisegment_pdus);
		}

		/* add this segment as the first one for this new pdu */
		fragment_add(tvb, deseg_offset, pinfo, msp->first_frame,
			tcp_fragment_table,
			0,
			nxtseq - deseg_seq,
			LT_SEQ(nxtseq, msp->nxtpdu));
		}
	}

	if (!called_dissector || pinfo->desegment_len != 0) {
		if (ipfd_head != NULL && ipfd_head->reassembled_in != 0 &&
		    !(ipfd_head->flags & FD_PARTIAL_REASSEMBLY)) {
			/*
			 * We know what frame this PDU is reassembled in;
			 * let the user know.
			 */
			item=proto_tree_add_uint(tcp_tree, hf_tcp_reassembled_in,
			    tvb, 0, 0, ipfd_head->reassembled_in);
			PROTO_ITEM_SET_GENERATED(item);
		}

		/*
		 * Either we didn't call the subdissector at all (i.e.,
		 * this is a segment that contains the middle of a
		 * higher-level PDU, but contains neither the beginning
		 * nor the end), or the subdissector couldn't dissect it
		 * all, as some data was missing (i.e., it set
		 * "pinfo->desegment_len" to the amount of additional
		 * data it needs).
		 */
		if (pinfo->desegment_offset == 0) {
			/*
			 * It couldn't, in fact, dissect any of it (the
			 * first byte it couldn't dissect is at an offset
			 * of "pinfo->desegment_offset" from the beginning
			 * of the payload, and that's 0).
			 * Just mark this as TCP.
			 */
			col_set_str(pinfo->cinfo, COL_PROTOCOL, "TCP");
			col_set_str(pinfo->cinfo, COL_INFO, "[TCP segment of a reassembled PDU]");
		}

		/*
		 * Show what's left in the packet as just raw TCP segment
		 * data.
		 * XXX - remember what protocol the last subdissector
		 * was, and report it as a continuation of that, instead?
		 */
		nbytes = tvb_reported_length_remaining(tvb, deseg_offset);
		proto_tree_add_text(tcp_tree, tvb, deseg_offset, -1,
		    "TCP segment data (%u byte%s)", nbytes,
		    plurality(nbytes, "", "s"));
	}
	pinfo->can_desegment=0;
	pinfo->desegment_offset = 0;
	pinfo->desegment_len = 0;

	if(another_pdu_follows){
		/* there was another pdu following this one. */
		pinfo->can_desegment=2;
		/* we also have to prevent the dissector from changing the
		 * PROTOCOL and INFO colums since what follows may be an
		 * incomplete PDU and we dont want it be changed back from
		 *  <Protocol>   to <TCP>
		 * XXX There is no good way to block the PROTOCOL column
		 * from being changed yet so we set the entire row unwritable.
		 * The flag cleared_writable stores the initial state.
		 */
		col_set_fence(pinfo->cinfo, COL_INFO);
		cleared_writable |= col_get_writable(pinfo->cinfo);
		col_set_writable(pinfo->cinfo, FALSE);
		offset += another_pdu_follows;
		seq += another_pdu_follows;
		goto again;
	} else {
		/* remove any blocking set above otherwise the
		 * proto,colinfo tap will break
		 */
		if(cleared_writable){
			col_set_writable(pinfo->cinfo, TRUE);
		}
	}
}

/*
 * Loop for dissecting PDUs within a TCP stream; assumes that a PDU
 * consists of a fixed-length chunk of data that contains enough information
 * to determine the length of the PDU, followed by rest of the PDU.
 *
 * The first three arguments are the arguments passed to the dissector
 * that calls this routine.
 *
 * "proto_desegment" is the dissector's flag controlling whether it should
 * desegment PDUs that cross TCP segment boundaries.
 *
 * "fixed_len" is the length of the fixed-length part of the PDU.
 *
 * "get_pdu_len()" is a routine called to get the length of the PDU from
 * the fixed-length part of the PDU; it's passed "pinfo", "tvb" and "offset".
 *
 * "dissect_pdu()" is the routine to dissect a PDU.
 */
void
tcp_dissect_pdus(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
		 gboolean proto_desegment, guint fixed_len,
		 guint (*get_pdu_len)(packet_info *, tvbuff_t *, int),
		 dissector_t dissect_pdu)
{
  volatile int offset = 0;
  int offset_before;
  guint length_remaining;
  guint plen;
  guint length;
  tvbuff_t *next_tvb;
  proto_item *item=NULL;

  while (tvb_reported_length_remaining(tvb, offset) != 0) {
    /*
     * We use "tvb_ensure_length_remaining()" to make sure there actually
     * *is* data remaining.  The protocol we're handling could conceivably
     * consists of a sequence of fixed-length PDUs, and therefore the
     * "get_pdu_len" routine might not actually fetch anything from
     * the tvbuff, and thus might not cause an exception to be thrown if
     * we've run past the end of the tvbuff.
     *
     * This means we're guaranteed that "length_remaining" is positive.
     */
    length_remaining = tvb_ensure_length_remaining(tvb, offset);

    /*
     * Can we do reassembly?
     */
    if (proto_desegment && pinfo->can_desegment) {
      /*
       * Yes - is the fixed-length part of the PDU split across segment
       * boundaries?
       */
      if (length_remaining < fixed_len) {
	/*
	 * Yes.  Tell the TCP dissector where the data for this message
	 * starts in the data it handed us, and how many more bytes we
	 * need, and return.
	 */
	pinfo->desegment_offset = offset;
	pinfo->desegment_len = DESEGMENT_ONE_MORE_SEGMENT;
	return;
      }
    }

    /*
     * Get the length of the PDU.
     */
    plen = (*get_pdu_len)(pinfo, tvb, offset);
    if (plen < fixed_len) {
      /*
       * Either:
       *
       *	1) the length value extracted from the fixed-length portion
       *	   doesn't include the fixed-length portion's length, and
       *	   was so large that, when the fixed-length portion's
       *	   length was added to it, the total length overflowed;
       *
       *	2) the length value extracted from the fixed-length portion
       *	   includes the fixed-length portion's length, and the value
       *	   was less than the fixed-length portion's length, i.e. it
       *	   was bogus.
       *
       * Report this as a bounds error.
       */
      show_reported_bounds_error(tvb, pinfo, tree);
      return;
    }
    /*
     * Display the PDU length as a field
     */
    item=proto_tree_add_uint(pinfo->tcp_tree, hf_tcp_pdu_size, tvb, offset, plen, plen);
    PROTO_ITEM_SET_GENERATED(item);



    /* give a hint to TCP where the next PDU starts
     * so that it can attempt to find it in case it starts
     * somewhere in the middle of a segment.
     */
    if(!pinfo->fd->flags.visited && tcp_analyze_seq){
       guint remaining_bytes;
       remaining_bytes=tvb_reported_length_remaining(tvb, offset);
       if(plen>remaining_bytes){
          pinfo->want_pdu_tracking=2;
          pinfo->bytes_until_next_pdu=plen-remaining_bytes;
       }
    }

    /*
     * Can we do reassembly?
     */
    if (proto_desegment && pinfo->can_desegment) {
      /*
       * Yes - is the PDU split across segment boundaries?
       */
      if (length_remaining < plen) {
	/*
	 * Yes.  Tell the TCP dissector where the data for this message
	 * starts in the data it handed us, and how many more bytes we
	 * need, and return.
	 */
	pinfo->desegment_offset = offset;
	pinfo->desegment_len = plen - length_remaining;
	return;
      }
    }

    /*
     * Construct a tvbuff containing the amount of the payload we have
     * available.  Make its reported length the amount of data in the PDU.
     *
     * XXX - if reassembly isn't enabled. the subdissector will throw a
     * BoundsError exception, rather than a ReportedBoundsError exception.
     * We really want a tvbuff where the length is "length", the reported
     * length is "plen", and the "if the snapshot length were infinite"
     * length is the minimum of the reported length of the tvbuff handed
     * to us and "plen", with a new type of exception thrown if the offset
     * is within the reported length but beyond that third length, with
     * that exception getting the "Unreassembled Packet" error.
     */
    length = length_remaining;
    if (length > plen)
        length = plen;
    next_tvb = tvb_new_subset(tvb, offset, length, plen);

    /*
     * Dissect the PDU.
     *
     * Catch the ReportedBoundsError exception; if this particular message
     * happens to get a ReportedBoundsError exception, that doesn't mean
     * that we should stop dissecting PDUs within this frame or chunk of
     * reassembled data.
     *
     * If it gets a BoundsError, we can stop, as there's nothing more to
     * see, so we just re-throw it.
     */
    TRY {
      (*dissect_pdu)(next_tvb, pinfo, tree);
    }
    CATCH(BoundsError) {
      RETHROW;
    }
    CATCH(ReportedBoundsError) {
     show_reported_bounds_error(tvb, pinfo, tree);
    }
    ENDTRY;

    /*
     * Step to the next PDU.
     * Make sure we don't overflow.
     */
    offset_before = offset;
    offset += plen;
    if (offset <= offset_before)
      break;
  }
}

static void
tcp_info_append_uint(packet_info *pinfo, const char *abbrev, guint32 val)
{
  if (check_col(pinfo->cinfo, COL_INFO))
    col_append_fstr(pinfo->cinfo, COL_INFO, " %s=%u", abbrev, val);
}

/* Supports the reporting the contents of a parsed SCPS capabilities vector */
static void
tcp_info_append_str(packet_info *pinfo, const char *abbrev, const char *val)
{
  if (check_col(pinfo->cinfo, COL_INFO))
    col_append_fstr(pinfo->cinfo, COL_INFO, " %s[%s]", abbrev, val);
}

static void
dissect_tcpopt_maxseg(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_item *hidden_item;
  guint16 mss;

  mss = tvb_get_ntohs(tvb, offset + 2);
  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_mss, tvb, offset,
				optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_uint_format(opt_tree, hf_tcp_option_mss_val, tvb, offset,
			     optlen, mss, "%s: %u bytes", optp->name, mss);
  tcp_info_append_uint(pinfo, "MSS", mss);
}

static void
dissect_tcpopt_wscale(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_item *hidden_item;
  guint8 ws;
  struct tcp_analysis *tcpd=NULL;

  tcpd=get_tcp_conversation_data(NULL,pinfo);

  ws = tvb_get_guint8(tvb, offset + 2);
  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_wscale, tvb,
				offset, optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_uint_format(opt_tree, hf_tcp_option_wscale_val, tvb,
			     offset, optlen, ws, "%s: %u (multiply by %u)",
			     optp->name, ws, 1 << ws);
  tcp_info_append_uint(pinfo, "WS", ws);
  if(!pinfo->fd->flags.visited && tcp_analyze_seq && tcp_relative_seq){
    pdu_store_window_scale_option(ws, tcpd);
  }
}

static void
dissect_tcpopt_sack(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_tree *field_tree = NULL;
  proto_item *tf=NULL;
  proto_item *hidden_item;
  guint32 leftedge, rightedge;
  struct tcp_analysis *tcpd=NULL;
  guint32 base_ack=0;

  if(tcp_analyze_seq && tcp_relative_seq){
    /* find(or create if needed) the conversation for this tcp session */
    tcpd=get_tcp_conversation_data(NULL,pinfo);

    if (tcpd) {
      base_ack=tcpd->rev->base_seq;
    }
  }

  tf = proto_tree_add_text(opt_tree, tvb, offset,      optlen, "%s:", optp->name);
  offset += 2;	/* skip past type and length */
  optlen -= 2;	/* subtract size of type and length */
  while (optlen > 0) {
    if (field_tree == NULL) {
      /* Haven't yet made a subtree out of this option.  Do so. */
      field_tree = proto_item_add_subtree(tf, *optp->subtree_index);
      hidden_item = proto_tree_add_boolean(field_tree, hf_tcp_option_sack, tvb,
				    offset, optlen, TRUE);
      PROTO_ITEM_SET_HIDDEN(hidden_item);
    }
    if (optlen < 4) {
      proto_tree_add_text(field_tree, tvb, offset,      optlen,
        "(suboption would go past end of option)");
      break;
    }
    leftedge = tvb_get_ntohl(tvb, offset)-base_ack;
    proto_tree_add_uint_format(field_tree, hf_tcp_option_sack_sle, tvb,
			       offset, 4, leftedge,
			       "left edge = %u%s", leftedge,
			       tcp_relative_seq ? " (relative)" : "");

    optlen -= 4;
    if (optlen < 4) {
      proto_tree_add_text(field_tree, tvb, offset,      optlen,
        "(suboption would go past end of option)");
      break;
    }
    /* XXX - check whether it goes past end of packet */
    rightedge = tvb_get_ntohl(tvb, offset + 4)-base_ack;
    optlen -= 4;
    proto_tree_add_uint_format(field_tree, hf_tcp_option_sack_sre, tvb,
			       offset+4, 4, rightedge,
			       "right edge = %u%s", rightedge,
			       tcp_relative_seq ? " (relative)" : "");
    tcp_info_append_uint(pinfo, "SLE", leftedge);
    tcp_info_append_uint(pinfo, "SRE", rightedge);
    proto_item_append_text(field_tree, " %u-%u", leftedge, rightedge);
    offset += 8;
  }
}

static void
dissect_tcpopt_echo(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_item *hidden_item;
  guint32 echo;

  echo = tvb_get_ntohl(tvb, offset + 2);
  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_echo, tvb, offset,
				optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_text(opt_tree, tvb, offset,      optlen,
			"%s: %u", optp->name, echo);
  tcp_info_append_uint(pinfo, "ECHO", echo);
}

static void
dissect_tcpopt_timestamp(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_item *hidden_item;
  guint32 tsv, tser;

  tsv = tvb_get_ntohl(tvb, offset + 2);
  tser = tvb_get_ntohl(tvb, offset + 6);
  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_time_stamp, tvb,
				offset, optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_text(opt_tree, tvb, offset,      optlen,
    "%s: TSval %u, TSecr %u", optp->name, tsv, tser);
  tcp_info_append_uint(pinfo, "TSV", tsv);
  tcp_info_append_uint(pinfo, "TSER", tser);
}

static void
dissect_tcpopt_cc(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  proto_item *hidden_item;
  guint32 cc;

  cc = tvb_get_ntohl(tvb, offset + 2);
  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_cc, tvb, offset,
				optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_text(opt_tree, tvb, offset,      optlen,
			"%s: %u", optp->name, cc);
  tcp_info_append_uint(pinfo, "CC", cc);
}

static void
dissect_tcpopt_qs(const ip_tcp_opt *optp, tvbuff_t *tvb,
    int offset, guint optlen, packet_info *pinfo, proto_tree *opt_tree)
{
  /* Quick-Start TCP option, as defined by RFC4782 */
  static const value_string qs_rates[] = {
    { 0, "0 bit/s"},
    { 1, "80 kbit/s"},
    { 2, "160 kbit/s"},
    { 3, "320 kbit/s"},
    { 4, "640 kbit/s"},
    { 5, "1.28 Mbit/s"},
    { 6, "2.56 Mbit/s"},
    { 7, "5.12 Mbit/s"},
    { 8, "10.24 Mbit/s"},
    { 9, "20.48 Mbit/s"},
    {10, "40.96 Mbit/s"},
    {11, "81.92 Mbit/s"},
    {12, "163.84 Mbit/s"},
    {13, "327.68 Mbit/s"},
    {14, "655.36 Mbit/s"},
    {15, "1.31072 Gbit/s"},
    {0, NULL}
  };
  proto_item *hidden_item;

  guint8 rate = tvb_get_guint8(tvb, offset + 2) & 0x0f;

  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_qs, tvb, offset,
				optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_text(opt_tree, tvb, offset,      optlen,
		      "%s: Rate response, %s, TTL diff %u ", optp->name,
		      val_to_str(rate, qs_rates, "Unknown"),
		      tvb_get_guint8(tvb, offset + 3));
  if (check_col(pinfo->cinfo, COL_INFO))
    col_append_fstr(pinfo->cinfo, COL_INFO, " QSresp=%s", val_to_str(rate, qs_rates, "Unknown"));
}


static void
dissect_tcpopt_scps(const ip_tcp_opt *optp, tvbuff_t *tvb,
		    int offset, guint optlen, packet_info *pinfo,
		    proto_tree *opt_tree)
{
  struct tcp_analysis *tcpd=NULL;
  proto_tree *field_tree = NULL;
  tcp_flow_t *flow;
  int         direction;
  proto_item *tf = NULL, *hidden_item;
  gchar       flags[64] = "<None>";
  gchar      *fstr[] = {"BETS", "SNACK1", "SNACK2", "COMP", "NLTS", "RESV1", "RESV2", "RESV3"};
  gint        i, bpos;
  guint8      capvector;
  guint8      connid;

  tcpd = get_tcp_conversation_data(NULL,pinfo);

  /* check direction and get ua lists */
  direction=CMP_ADDRESS(&pinfo->src, &pinfo->dst);

  /* if the addresses are equal, match the ports instead */
  if(direction==0) {
    direction= (pinfo->srcport > pinfo->destport) ? 1 : -1;
  }

  if(direction>=0)
    flow =&(tcpd->flow1);
  else
    flow =&(tcpd->flow2);

  /* If the option length == 4, this is a real SCPS capability option
   * See "CCSDS 714.0-B-2 (CCSDS Recommended Standard for SCPS Transport Protocol
   * (SCPS-TP)" Section 3.2.3 for definition.
   */
  if (optlen == 4) {
    capvector = tvb_get_guint8(tvb, offset + 2);
    flags[0] = '\0';

    /* Decode the capabilities vector for display */
    for (i = 0; i < 5; i++) {
      bpos = 128 >> i;
      if (capvector & bpos) {
		if (flags[0]) {
		  g_strlcat(flags, ", ", 64);
		}
		g_strlcat(flags, fstr[i], 64);
      }
    }

    /* If lossless header compression is offered, there will be a
     * single octet connectionId following the capabilities vector
     */
    if (capvector & 0x10)
      connid    = tvb_get_guint8(tvb, offset + 3);
    else
      connid    = 0;

    tf = proto_tree_add_uint_format(opt_tree, hf_tcp_option_scps_vector, tvb,
				    offset, optlen, capvector,
				    "%s: 0x%02x (%s)",
				    optp->name, capvector, flags);
    hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_scps,
					tvb, offset, optlen, TRUE);
    PROTO_ITEM_SET_HIDDEN(hidden_item);

    field_tree = proto_item_add_subtree(tf, ett_tcp_option_scps);

    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_bets, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_snack1, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_snack2, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_compress, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_nlts, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_resv1, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_resv2, tvb,
			   offset + 13, 1, capvector);
    proto_tree_add_boolean(field_tree, hf_tcp_scpsoption_flags_resv3, tvb,
			   offset + 13, 1, capvector);

    tcp_info_append_str(pinfo, "SCPS", flags);

    flow->scps_capable = 1;

    if (connid)
      tcp_info_append_uint(pinfo, "Connection ID", connid);
  }
  else {
    /* The option length != 4, so this is an infamous "extended capabilities
     * option. See "CCSDS 714.0-B-2 (CCSDS Recommended Standard for SCPS
     * Transport Protocol (SCPS-TP)" Section 3.2.5 for definition.
     *
     *  As the format of this option is only partially defined (it is
     * a community (or more likely vendor) defined format beyond that, so
     * at least for now, we only parse the standardized portion of the option.
     */
    guint8 local_offset = 2;
    guint8 binding_space;
    guint8 extended_cap_length;

    if (flow->scps_capable != 1) {
      /* There was no SCPS capabilities option preceeding this */
      tf = proto_tree_add_uint_format(opt_tree, hf_tcp_option_scps_vector,
				      tvb, offset, optlen, 0, "%s: (%d %s)",
				      "Illegal SCPS Extended Capabilities",
				      (optlen),
				      "bytes");
    }
    else {
      tf = proto_tree_add_uint_format(opt_tree, hf_tcp_option_scps_vector,
				      tvb, offset, optlen, 0, "%s: (%d %s)",
				      "SCPS Extended Capabilities",
				      (optlen),
				      "bytes");
      field_tree=proto_item_add_subtree(tf, ett_tcp_option_scps_extended);
      /* There may be multiple binding spaces included in a single option,
       * so we will semi-parse each of the stacked binding spaces - skipping
       * over the octets following the binding space identifier and length.
       */

      while (optlen > local_offset) {
		  proto_item *hidden_item;

	/* 1st octet is Extended Capability Binding Space */
	binding_space = tvb_get_guint8(tvb, (offset + local_offset));

	/* 2nd octet (upper 4-bits) has binding space length in 16-bit words.
	 * As defined by the specification, this length is exclusive of the
	 * octets containing the extended capability type and length
	 */

	extended_cap_length =
	  (tvb_get_guint8(tvb, (offset + local_offset + 1)) >> 4);

	/* Convert the extended capabilities length into bytes for display */
	extended_cap_length = (extended_cap_length << 1);

	proto_tree_add_text(field_tree, tvb, offset + local_offset, 2,
			    "\tBinding Space %u",
			    binding_space);
	hidden_item = proto_tree_add_uint(field_tree, hf_tcp_option_scps_binding,
				   tvb, (offset + local_offset), 1,
				   binding_space);

	PROTO_ITEM_SET_HIDDEN(hidden_item);

	/* Step past the binding space and length octets */
	local_offset += 2;

	proto_tree_add_text(field_tree, tvb, offset + local_offset,
			    extended_cap_length,
			    "\tBinding Space Data (%u bytes)",
			    extended_cap_length);

	tcp_info_append_uint(pinfo, "EXCAP", binding_space);

	/* Step past the Extended capability data
	 * Treat the extended capability data area as opaque;
	 * If one desires to parse the extended capability data
	 * (say, in a vendor aware build of wireshark), it would
	 * be trigged here.
	 */
	local_offset += extended_cap_length;
      }
    }
  }
}

/* This is called for SYN+ACK packets and the purpose is to verify that
 * the SCPS capabilities option has been successfully negotiated for the flow.
 * If the SCPS capabilities option was offered by only one party, the
 * proactively set scps_capable attribute of the flow (set upon seeing
 * the first instance of the SCPS option) is revoked.
 */
static void
verify_scps(packet_info *pinfo,  proto_item *tf_syn, struct tcp_analysis *tcpd)
{
  tf_syn = 0x0;

  if(tcpd) {
    if ((!(tcpd->flow1.scps_capable)) || (!(tcpd->flow2.scps_capable))) {
      tcpd->flow1.scps_capable = 0;
      tcpd->flow2.scps_capable = 0;
    }
    else {
      expert_add_info_format(pinfo, tf_syn, PI_SEQUENCE, PI_NOTE,
			     "Connection establish request (SYN-ACK): SCPS Capabilities Negotiated");
    }
  }
}

/* See "CCSDS 714.0-B-2 (CCSDS Recommended Standard for SCPS
 * Transport Protocol (SCPS-TP)" Section 3.5 for definition of the SNACK option
 */
static void
dissect_tcpopt_snack(const ip_tcp_opt *optp, tvbuff_t *tvb,
		    int offset, guint optlen, packet_info *pinfo,
		    proto_tree *opt_tree)
{
  struct tcp_analysis *tcpd=NULL;
  guint16 relative_hole_offset;
  guint16 relative_hole_size;
  guint16 base_mss = 0;
  guint32 ack;
  guint32 hole_start;
  guint32 hole_end;
  char    null_modifier[] = "\0";
  char    relative_modifier[] = "(relative)";
  char   *modifier = null_modifier;
  proto_item *hidden_item;

  tcpd = get_tcp_conversation_data(NULL,pinfo);

  /* The SNACK option reports missing data with a granualarity of segments. */
  relative_hole_offset = tvb_get_ntohs(tvb, offset + 2);
  relative_hole_size = tvb_get_ntohs(tvb, offset + 4);

  hidden_item = proto_tree_add_boolean(opt_tree, hf_tcp_option_snack, tvb,
				offset, optlen, TRUE);
  PROTO_ITEM_SET_HIDDEN(hidden_item);

  hidden_item = proto_tree_add_uint(opt_tree, hf_tcp_option_snack_offset,
			       tvb, offset, optlen, relative_hole_offset);
  PROTO_ITEM_SET_HIDDEN(hidden_item);

  hidden_item = proto_tree_add_uint(opt_tree, hf_tcp_option_snack_size,
			       tvb, offset, optlen, relative_hole_size);
  PROTO_ITEM_SET_HIDDEN(hidden_item);
  proto_tree_add_text(opt_tree, tvb, offset, optlen,
		      "%s: Offset %u, Size %u", optp->name,
		      relative_hole_offset, relative_hole_size);

  ack   = tvb_get_ntohl(tvb, 8);

  if (tcp_relative_seq) {
    ack -= tcpd->rev->base_seq;
    modifier = relative_modifier;
  }

  /* To aid analysis, we can use a simple but generally effective heuristic
   * to report the most likely boundaries of the missing data.  If the
   * flow is scps_capable, we track the maximum sized segment that was
   * acknowledged by the receiver and use that as the reporting granularity.
   * This may be different from the negotiated MTU due to PMTUD or flows
   * that do not send max-sized segments.
   */
  base_mss = tcpd->fwd->maxsizeacked;

  if (base_mss) {
    proto_item *hidden_item;
    /* Scale the reported offset and hole size by the largest segment acked */
    hole_start = ack + (base_mss * relative_hole_offset);
    hole_end   = hole_start + (base_mss * relative_hole_size);

    hidden_item = proto_tree_add_uint(opt_tree, hf_tcp_option_snack_le,
			       tvb, offset, optlen, hole_start);
	PROTO_ITEM_SET_HIDDEN(hidden_item);

    hidden_item = proto_tree_add_uint(opt_tree, hf_tcp_option_snack_re,
			       tvb, offset, optlen, hole_end);
	PROTO_ITEM_SET_HIDDEN(hidden_item);
    proto_tree_add_text(opt_tree, tvb, offset, optlen,
			"\tMissing Sequence %u - %u %s",
			hole_start, hole_end, modifier);

    tcp_info_append_uint(pinfo, "SNLE", hole_start);
    tcp_info_append_uint(pinfo, "SNRE", hole_end);

    expert_add_info_format(pinfo, NULL, PI_SEQUENCE, PI_NOTE,
			   "SNACK Sequence %u - %u %s",
			   hole_start, hole_end, modifier);
  }
}

static const ip_tcp_opt tcpopts[] = {
  {
    TCPOPT_EOL,
    "EOL",
    NULL,
    NO_LENGTH,
    0,
    NULL,
  },
  {
    TCPOPT_NOP,
    "NOP",
    NULL,
    NO_LENGTH,
    0,
    NULL,
  },
  {
    TCPOPT_MSS,
    "Maximum segment size",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_MSS,
    dissect_tcpopt_maxseg
  },
  {
    TCPOPT_WINDOW,
    "Window scale",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_WINDOW,
    dissect_tcpopt_wscale
  },
  {
    TCPOPT_SACK_PERM,
    "SACK permitted",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_SACK_PERM,
    NULL,
  },
  {
    TCPOPT_SACK,
    "SACK",
    &ett_tcp_option_sack,
    VARIABLE_LENGTH,
    TCPOLEN_SACK_MIN,
    dissect_tcpopt_sack
  },
  {
    TCPOPT_ECHO,
    "Echo",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_ECHO,
    dissect_tcpopt_echo
  },
  {
    TCPOPT_ECHOREPLY,
    "Echo reply",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_ECHOREPLY,
    dissect_tcpopt_echo
  },
  {
    TCPOPT_TIMESTAMP,
    "Timestamps",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_TIMESTAMP,
    dissect_tcpopt_timestamp
  },
  {
    TCPOPT_CC,
    "CC",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_CC,
    dissect_tcpopt_cc
  },
  {
    TCPOPT_CCNEW,
    "CC.NEW",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_CCNEW,
    dissect_tcpopt_cc
  },
  {
    TCPOPT_CCECHO,
    "CC.ECHO",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_CCECHO,
    dissect_tcpopt_cc
  },
  {
    TCPOPT_MD5,
    "TCP MD5 signature",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_MD5,
    NULL
  },
  {
    TCPOPT_SCPS,
    "SCPS capabilities",
    &ett_tcp_option_scps,
    VARIABLE_LENGTH,
    TCPOLEN_SCPS,
    dissect_tcpopt_scps
  },
  {
    TCPOPT_SNACK,
    "Selective Negative Acknowledgement",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_SNACK,
    dissect_tcpopt_snack
  },
  {
    TCPOPT_RECBOUND,
    "SCPS record boundary",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_RECBOUND,
    NULL
  },
  {
    TCPOPT_CORREXP,
    "SCPS corruption experienced",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_CORREXP,
    NULL
  },
  {
    TCPOPT_QS,
    "Quick-Start",
    NULL,
    FIXED_LENGTH,
    TCPOLEN_QS,
    dissect_tcpopt_qs
  }
};

#define N_TCP_OPTS	(sizeof tcpopts / sizeof tcpopts[0])

/* Determine if there is a sub-dissector and call it; return TRUE
   if there was a sub-dissector, FALSE otherwise.

   This has been separated into a stand alone routine to other protocol
   dissectors can call to it, e.g., SOCKS. */

static gboolean try_heuristic_first = FALSE;


/* this function can be called with tcpd==NULL as from the msproxy dissector */
gboolean
decode_tcp_ports(tvbuff_t *tvb, int offset, packet_info *pinfo,
	proto_tree *tree, int src_port, int dst_port,
	struct tcp_analysis *tcpd)
{
  tvbuff_t *next_tvb;
  int low_port, high_port;
  int save_desegment_offset;
  guint32 save_desegment_len;

  /* dont call subdissectors for keepalive or zerowindowprobes
   * even though they do contain payload "data"
   * keeaplives just contain garbage and zwp contain too little data (1 byte)
   * so why bother.
   */
  if(tcpd && tcpd->ta){
    if(tcpd->ta->flags&(TCP_A_ZERO_WINDOW_PROBE|TCP_A_KEEP_ALIVE)){
      return TRUE;
    }
  }

  next_tvb = tvb_new_subset_remaining(tvb, offset);

/* determine if this packet is part of a conversation and call dissector */
/* for the conversation if available */

  if (try_conversation_dissector(&pinfo->src, &pinfo->dst, PT_TCP,
		src_port, dst_port, next_tvb, pinfo, tree)){
    pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
    return TRUE;
  }

  if (try_heuristic_first) {
    /* do lookup with the heuristic subdissector table */
    save_desegment_offset = pinfo->desegment_offset;
    save_desegment_len = pinfo->desegment_len;
    if (dissector_try_heuristic(heur_subdissector_list, next_tvb, pinfo, tree)){
       pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
       return TRUE;
    }
    /*
     * They rejected the packet; make sure they didn't also request
     * desegmentation (we could just override the request, but
     * rejecting a packet *and* requesting desegmentation is a sign
     * of the dissector's code needing clearer thought, so we fail
     * so that the problem is made more obvious).
     */
    DISSECTOR_ASSERT(save_desegment_offset == pinfo->desegment_offset &&
                     save_desegment_len == pinfo->desegment_len);
  }

  /* Do lookups with the subdissector table.
     We try the port number with the lower value first, followed by the
     port number with the higher value.  This means that, for packets
     where a dissector is registered for *both* port numbers:

	1) we pick the same dissector for traffic going in both directions;

	2) we prefer the port number that's more likely to be the right
	   one (as that prefers well-known ports to reserved ports);

     although there is, of course, no guarantee that any such strategy
     will always pick the right port number.

     XXX - we ignore port numbers of 0, as some dissectors use a port
     number of 0 to disable the port. */
  if (src_port > dst_port) {
    low_port = dst_port;
    high_port = src_port;
  } else {
    low_port = src_port;
    high_port = dst_port;
  }
  if (low_port != 0 &&
      dissector_try_port(subdissector_table, low_port, next_tvb, pinfo, tree)){
    pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
    return TRUE;
  }
  if (high_port != 0 &&
      dissector_try_port(subdissector_table, high_port, next_tvb, pinfo, tree)){
    pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
    return TRUE;
  }

  if (!try_heuristic_first) {
    /* do lookup with the heuristic subdissector table */
    save_desegment_offset = pinfo->desegment_offset;
    save_desegment_len = pinfo->desegment_len;
    if (dissector_try_heuristic(heur_subdissector_list, next_tvb, pinfo, tree)){
       pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
       return TRUE;
    }
    /*
     * They rejected the packet; make sure they didn't also request
     * desegmentation (we could just override the request, but
     * rejecting a packet *and* requesting desegmentation is a sign
     * of the dissector's code needing clearer thought, so we fail
     * so that the problem is made more obvious).
     */
    DISSECTOR_ASSERT(save_desegment_offset == pinfo->desegment_offset &&
                     save_desegment_len == pinfo->desegment_len);
  }

  /* Oh, well, we don't know this; dissect it as data. */
  call_dissector(data_handle,next_tvb, pinfo, tree);

  pinfo->want_pdu_tracking -= !!(pinfo->want_pdu_tracking);
  return FALSE;
}

static void
process_tcp_payload(tvbuff_t *tvb, volatile int offset, packet_info *pinfo,
	proto_tree *tree, proto_tree *tcp_tree, int src_port, int dst_port,
	guint32 seq, guint32 nxtseq, gboolean is_tcp_segment,
	struct tcp_analysis *tcpd)
{
	pinfo->want_pdu_tracking=0;

	TRY {
		if(is_tcp_segment){
			/*qqq   see if it is an unaligned PDU */
			if(tcpd && tcp_analyze_seq && (!tcp_desegment)){
				if(seq || nxtseq){
					offset=scan_for_next_pdu(tvb, tcp_tree, pinfo, offset,
						seq, nxtseq, tcpd->fwd->multisegment_pdus);
				}
			}
		}
		/* if offset is -1 this means that this segment is known
		 * to be fully inside a previously detected pdu
		 * so we dont even need to try to dissect it either.
		 */
		if( (offset!=-1) &&
		    decode_tcp_ports(tvb, offset, pinfo, tree, src_port,
		        dst_port, tcpd) ){
			/*
			 * We succeeded in handing off to a subdissector.
			 *
			 * Is this a TCP segment or a reassembled chunk of
			 * TCP payload?
			 */
			if(is_tcp_segment){
				/* if !visited, check want_pdu_tracking and
				   store it in table */
				if(tcpd && (!pinfo->fd->flags.visited) &&
				    tcp_analyze_seq && pinfo->want_pdu_tracking){
					if(seq || nxtseq){
						pdu_store_sequencenumber_of_next_pdu(
						    pinfo,
						    seq,
						    nxtseq+pinfo->bytes_until_next_pdu,
						    tcpd->fwd->multisegment_pdus);
					}
				}
			}
		}
	}
	CATCH_ALL {
		/* We got an exception. At this point the dissection is
		 * completely aborted and execution will be transfered back
		 * to (probably) the frame dissector.
		 * Here we have to place whatever we want the dissector
		 * to do before aborting the tcp dissection.
		 */
		/*
		 * Is this a TCP segment or a reassembled chunk of TCP
		 * payload?
		 */
		if(is_tcp_segment){
			/*
			 * It's from a TCP segment.
			 *
			 * if !visited, check want_pdu_tracking and store it
			 * in table
			 */
			if(tcpd && (!pinfo->fd->flags.visited) && tcp_analyze_seq && pinfo->want_pdu_tracking){
				if(seq || nxtseq){
					pdu_store_sequencenumber_of_next_pdu(pinfo,
					    seq,
					    nxtseq+pinfo->bytes_until_next_pdu,
					    tcpd->fwd->multisegment_pdus);
				}
			}
		}
		RETHROW;
	}
	ENDTRY;
}

void
dissect_tcp_payload(tvbuff_t *tvb, packet_info *pinfo, int offset, guint32 seq,
		    guint32 nxtseq, guint32 sport, guint32 dport,
		    proto_tree *tree, proto_tree *tcp_tree,
		    struct tcp_analysis *tcpd)
{
  gboolean save_fragmented;

  /* Can we desegment this segment? */
  if (pinfo->can_desegment) {
    /* Yes. */
    desegment_tcp(tvb, pinfo, offset, seq, nxtseq, sport, dport, tree,
        tcp_tree, tcpd);
  } else {
    /* No - just call the subdissector.
       Mark this as fragmented, so if somebody throws an exception,
       we don't report it as a malformed frame. */
    save_fragmented = pinfo->fragmented;
    pinfo->fragmented = TRUE;
    process_tcp_payload(tvb, offset, pinfo, tree, tcp_tree, sport, dport,
        seq, nxtseq, TRUE, tcpd);
    pinfo->fragmented = save_fragmented;
  }
}

static void
dissect_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
  guint8  th_off_x2; /* combines th_off and th_x2 */
  guint16 th_sum;
  guint16 th_urp;
  proto_tree *tcp_tree = NULL, *field_tree = NULL;
  proto_item *ti = NULL, *tf, *hidden_item;
  int        offset = 0;
  emem_strbuf_t *flags_strbuf = ep_strbuf_new_label("<None>");
  const gchar *fstr[] = {"FIN", "SYN", "RST", "PSH", "ACK", "URG", "ECN", "CWR"};
  gint       i;
  guint      bpos;
  guint      optlen;
  guint32    nxtseq = 0;
  guint      reported_len;
  vec_t      cksum_vec[4];
  guint32    phdr[2];
  guint16    computed_cksum;
  guint16    real_window;
  guint      length_remaining;
  gboolean   desegment_ok;
  struct tcpinfo tcpinfo;
  struct tcpheader *tcph;
  proto_item *tf_syn = NULL, *tf_fin = NULL, *tf_rst = NULL;
  conversation_t *conv=NULL;
  struct tcp_analysis *tcpd=NULL;
  struct tcp_per_packet_data_t *tcppd=NULL;
  proto_item *item;
  proto_tree *checksum_tree;

  tcph=ep_alloc(sizeof(struct tcpheader));
  SET_ADDRESS(&tcph->ip_src, pinfo->src.type, pinfo->src.len, pinfo->src.data);
  SET_ADDRESS(&tcph->ip_dst, pinfo->dst.type, pinfo->dst.len, pinfo->dst.data);

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "TCP");

  /* Clear out the Info column. */
  col_clear(pinfo->cinfo, COL_INFO);

  tcph->th_sport = tvb_get_ntohs(tvb, offset);
  tcph->th_dport = tvb_get_ntohs(tvb, offset + 2);
  if (check_col(pinfo->cinfo, COL_INFO)) {
    col_append_fstr(pinfo->cinfo, COL_INFO, "%s > %s",
      get_tcp_port(tcph->th_sport), get_tcp_port(tcph->th_dport));
  }
  if (tree) {
    if (tcp_summary_in_tree) {
	    ti = proto_tree_add_protocol_format(tree, proto_tcp, tvb, 0, -1,
		"Transmission Control Protocol, Src Port: %s (%u), Dst Port: %s (%u)",
		get_tcp_port(tcph->th_sport), tcph->th_sport,
		get_tcp_port(tcph->th_dport), tcph->th_dport);
    }
    else {
	    ti = proto_tree_add_item(tree, proto_tcp, tvb, 0, -1, FALSE);
    }
    tcp_tree = proto_item_add_subtree(ti, ett_tcp);
    pinfo->tcp_tree=tcp_tree;

    proto_tree_add_uint_format(tcp_tree, hf_tcp_srcport, tvb, offset, 2, tcph->th_sport,
	"Source port: %s (%u)", get_tcp_port(tcph->th_sport), tcph->th_sport);
    proto_tree_add_uint_format(tcp_tree, hf_tcp_dstport, tvb, offset + 2, 2, tcph->th_dport,
	"Destination port: %s (%u)", get_tcp_port(tcph->th_dport), tcph->th_dport);
    hidden_item = proto_tree_add_uint(tcp_tree, hf_tcp_port, tvb, offset, 2, tcph->th_sport);
    PROTO_ITEM_SET_HIDDEN(hidden_item);
    hidden_item = proto_tree_add_uint(tcp_tree, hf_tcp_port, tvb, offset + 2, 2, tcph->th_dport);
    PROTO_ITEM_SET_HIDDEN(hidden_item);

    /*  If we're dissecting the headers of a TCP packet in an ICMP packet
     *  then go ahead and put the sequence numbers in the tree now (because
     *  they won't be put in later because the ICMP packet only contains up
     *  to the sequence number).
     *  We should only need to do this for IPv4 since IPv6 will hopefully
     *  carry enough TCP payload for this dissector to put the sequence
     *  numbers in via the regular code path.
     */
    if (pinfo->layer_names != NULL && pinfo->layer_names->str != NULL) {
      /*  use strstr because g_strrstr is only present in glib2.0 and
       *  g_str_has_suffix in glib2.2
       */
      if (strstr(pinfo->layer_names->str, "icmp:ip") != NULL)
		proto_tree_add_item(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, FALSE);
    }
  }

  /* Set the source and destination port numbers as soon as we get them,
     so that they're available to the "Follow TCP Stream" code even if
     we throw an exception dissecting the rest of the TCP header. */
  pinfo->ptype = PT_TCP;
  pinfo->srcport = tcph->th_sport;
  pinfo->destport = tcph->th_dport;

  tcph->th_seq = tvb_get_ntohl(tvb, offset + 4);
  tcph->th_ack = tvb_get_ntohl(tvb, offset + 8);
  th_off_x2 = tvb_get_guint8(tvb, offset + 12);
  tcph->th_flags = tvb_get_guint8(tvb, offset + 13);
  tcph->th_win = tvb_get_ntohs(tvb, offset + 14);
  real_window = tcph->th_win;
  tcph->th_hlen = hi_nibble(th_off_x2) * 4;  /* TCP header length, in bytes */

  /* find(or create if needed) the conversation for this tcp session */
  conv=get_tcp_conversation(pinfo);
  tcpd=get_tcp_conversation_data(conv,pinfo);

  item = proto_tree_add_uint(tcp_tree, hf_tcp_stream, tvb, offset, 0, conv->index);
  PROTO_ITEM_SET_GENERATED(item);

  /* If this is a SYN packet, then check if it's seq-nr is different
   * from the base_seq of the retrieved conversation. If this is the
   * case, create a new conversation with the same addresses and ports
   * and set the TA_PORTS_REUSED flag. If the seq-nr is the same as
   * the base_seq, then do nothing so it will be marked as a retrans-
   * mission later.
   */
  if(tcpd && ((tcph->th_flags&(TH_SYN|TH_ACK))==TH_SYN) &&
      (tcpd->fwd->base_seq!=0) &&
      (tcph->th_seq!=tcpd->fwd->base_seq) ) {
    if (!(pinfo->fd->flags.visited)) {
      conv=conversation_new(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype, pinfo->srcport, pinfo->destport, 0);
      tcpd=get_tcp_conversation_data(conv,pinfo);
    }
    if(!tcpd->ta)
      tcp_analyze_get_acked_struct(pinfo->fd->num, TRUE, tcpd);
    tcpd->ta->flags|=TCP_A_REUSED_PORTS;
  }


  /* Do we need to calculate timestamps relative to the tcp-stream? */
  if (tcp_calculate_ts) {
    tcppd = p_get_proto_data(pinfo->fd, proto_tcp);

    /*
     * Calculate the timestamps relative to this conversation (but only on the
     * first run when frames are accessed sequentially)
     */
    if (!(pinfo->fd->flags.visited))
      tcp_calculate_timestamps(pinfo, tcpd, tcppd);
  }

  /*
   * If we've been handed an IP fragment, we don't know how big the TCP
   * segment is, so don't do anything that requires that we know that.
   *
   * The same applies if we're part of an error packet.  (XXX - if the
   * ICMP and ICMPv6 dissectors could set a "this is how big the IP
   * header says it is" length in the tvbuff, we could use that; such
   * a length might also be useful for handling packets where the IP
   * length is bigger than the actual data available in the frame; the
   * dissectors should trust that length, and then throw a
   * ReportedBoundsError exception when they go past the end of the frame.)
   *
   * We also can't determine the segment length if the reported length
   * of the TCP packet is less than the TCP header length.
   */
  reported_len = tvb_reported_length(tvb);

  if (!pinfo->fragmented && !pinfo->in_error_pkt) {
    if (reported_len < tcph->th_hlen) {
      proto_item *pi;
      pi = proto_tree_add_text(tcp_tree, tvb, offset, 0,
        "Short segment. Segment/fragment does not contain a full TCP header"
        " (might be NMAP or someone else deliberately sending unusual packets)");
      PROTO_ITEM_SET_GENERATED(pi);
      expert_add_info_format(pinfo, pi, PI_MALFORMED, PI_WARN, "Short segment");
      tcph->th_have_seglen = FALSE;
    } else {
      /* Compute the length of data in this segment. */
      tcph->th_seglen = reported_len - tcph->th_hlen;
      tcph->th_have_seglen = TRUE;

      if (tree) { /* Add the seglen as an invisible field */

        hidden_item = proto_tree_add_uint(ti, hf_tcp_len, tvb, offset+12, 1, tcph->th_seglen);
		PROTO_ITEM_SET_HIDDEN(hidden_item);

      }


      /* handle TCP seq# analysis parse all new segments we see */
      if(tcp_analyze_seq){
          if(!(pinfo->fd->flags.visited)){
              tcp_analyze_sequence_number(pinfo, tcph->th_seq, tcph->th_ack, tcph->th_seglen, tcph->th_flags, tcph->th_win, tcpd);
          }
          if(tcp_relative_seq){
              tcp_get_relative_seq_ack(&(tcph->th_seq), &(tcph->th_ack), &(tcph->th_win), tcpd);
          }
      }

      /* Compute the sequence number of next octet after this segment. */
      nxtseq = tcph->th_seq + tcph->th_seglen;
    }
  } else
    tcph->th_have_seglen = FALSE;

  if (check_col(pinfo->cinfo, COL_INFO) || tree) {
    gboolean first_flag = TRUE;
    for (i = 0; i < 8; i++) {
      bpos = 1 << i;
      if (tcph->th_flags & bpos) {
        if (first_flag) {
          ep_strbuf_truncate(flags_strbuf, 0);
        }
        ep_strbuf_append_printf(flags_strbuf, "%s%s", first_flag ? "" : ", ", fstr[i]);
        first_flag = FALSE;
      }
    }
  }

  if (check_col(pinfo->cinfo, COL_INFO)) {
    col_append_fstr(pinfo->cinfo, COL_INFO, " [%s] Seq=%u", flags_strbuf->str, tcph->th_seq);
    if (tcph->th_flags&TH_ACK) {
      col_append_fstr(pinfo->cinfo, COL_INFO, " Ack=%u", tcph->th_ack);
    }
    if (tcph->th_flags&TH_SYN) {   /* SYNs are never scaled */
      col_append_fstr(pinfo->cinfo, COL_INFO, " Win=%u", real_window);
    } else {
      col_append_fstr(pinfo->cinfo, COL_INFO, " Win=%u", tcph->th_win);
    }
  }

  if (tree) {
    if (tcp_summary_in_tree) {
      proto_item_append_text(ti, ", Seq: %u", tcph->th_seq);
    }
    if(tcp_relative_seq){
      proto_tree_add_uint_format(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, tcph->th_seq, "Sequence number: %u    (relative sequence number)", tcph->th_seq);
    } else {
      proto_tree_add_uint(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, tcph->th_seq);
    }
  }

  if (tcph->th_hlen < TCPH_MIN_LEN) {
    /* Give up at this point; we put the source and destination port in
       the tree, before fetching the header length, so that they'll
       show up if this is in the failing packet in an ICMP error packet,
       but it's now time to give up if the header length is bogus. */
    if (check_col(pinfo->cinfo, COL_INFO))
      col_append_fstr(pinfo->cinfo, COL_INFO, ", bogus TCP header length (%u, must be at least %u)",
        tcph->th_hlen, TCPH_MIN_LEN);
    if (tree) {
      proto_tree_add_uint_format(tcp_tree, hf_tcp_hdr_len, tvb, offset + 12, 1, tcph->th_hlen,
       "Header length: %u bytes (bogus, must be at least %u)", tcph->th_hlen,
       TCPH_MIN_LEN);
    }
    return;
  }

  if (tree) {
    if (tcp_summary_in_tree) {
      if(tcph->th_flags&TH_ACK){
        proto_item_append_text(ti, ", Ack: %u", tcph->th_ack);
      }
      if (tcph->th_have_seglen)
        proto_item_append_text(ti, ", Len: %u", tcph->th_seglen);
    }
    proto_item_set_len(ti, tcph->th_hlen);
    if (tcph->th_have_seglen) {
      if (nxtseq != tcph->th_seq) {
        if(tcp_relative_seq){
          tf=proto_tree_add_uint_format(tcp_tree, hf_tcp_nxtseq, tvb, offset, 0, nxtseq, "Next sequence number: %u    (relative sequence number)", nxtseq);
        } else {
          tf=proto_tree_add_uint(tcp_tree, hf_tcp_nxtseq, tvb, offset, 0, nxtseq);
        }
        PROTO_ITEM_SET_GENERATED(tf);
      }
    }
    if (tcph->th_flags & TH_ACK) {
      if(tcp_relative_seq){
        proto_tree_add_uint_format(tcp_tree, hf_tcp_ack, tvb, offset + 8, 4, tcph->th_ack, "Acknowledgement number: %u    (relative ack number)", tcph->th_ack);
      } else {
        proto_tree_add_uint(tcp_tree, hf_tcp_ack, tvb, offset + 8, 4, tcph->th_ack);
      }
    } else {
      /* Verify that the ACK field is zero */
      if(tvb_get_ntohl(tvb, offset+8) != 0){
	proto_tree_add_text(tcp_tree, tvb, offset+8, 4,"Acknowledgement number: Broken TCP. The acknowledge field is nonzero while the ACK flag is not set");
      }
    }
    proto_tree_add_uint_format(tcp_tree, hf_tcp_hdr_len, tvb, offset + 12, 1, tcph->th_hlen,
	"Header length: %u bytes", tcph->th_hlen);
    tf = proto_tree_add_uint_format(tcp_tree, hf_tcp_flags, tvb, offset + 13, 1,
	tcph->th_flags, "Flags: 0x%02x (%s)", tcph->th_flags, flags_strbuf->str);
    field_tree = proto_item_add_subtree(tf, ett_tcp_flags);
    proto_tree_add_boolean(field_tree, hf_tcp_flags_cwr, tvb, offset + 13, 1, tcph->th_flags);
    proto_tree_add_boolean(field_tree, hf_tcp_flags_ecn, tvb, offset + 13, 1, tcph->th_flags);
    proto_tree_add_boolean(field_tree, hf_tcp_flags_urg, tvb, offset + 13, 1, tcph->th_flags);
    proto_tree_add_boolean(field_tree, hf_tcp_flags_ack, tvb, offset + 13, 1, tcph->th_flags);
    proto_tree_add_boolean(field_tree, hf_tcp_flags_push, tvb, offset + 13, 1, tcph->th_flags);
    tf_rst = proto_tree_add_boolean(field_tree, hf_tcp_flags_reset, tvb, offset + 13, 1, tcph->th_flags);
    tf_syn = proto_tree_add_boolean(field_tree, hf_tcp_flags_syn, tvb, offset + 13, 1, tcph->th_flags);
    tf_fin = proto_tree_add_boolean(field_tree, hf_tcp_flags_fin, tvb, offset + 13, 1, tcph->th_flags);
    if(tcp_relative_seq
    && (tcph->th_win!=real_window)
    && !(tcph->th_flags&TH_SYN) ){   /* SYNs are never scaled */
      proto_tree_add_uint_format(tcp_tree, hf_tcp_window_size, tvb, offset + 14, 2, tcph->th_win, "Window size: %u (scaled)", tcph->th_win);
    } else {
      proto_tree_add_uint(tcp_tree, hf_tcp_window_size, tvb, offset + 14, 2, real_window);
    }
  }

  if(tcph->th_flags & TH_SYN) {
    if(tcph->th_flags & TH_ACK)
      expert_add_info_format(pinfo, tf_syn, PI_SEQUENCE, PI_CHAT, "Connection establish acknowledge (SYN+ACK): server port %s",
                             get_tcp_port(tcph->th_sport));
    else
      expert_add_info_format(pinfo, tf_syn, PI_SEQUENCE, PI_CHAT, "Connection establish request (SYN): server port %s",
                             get_tcp_port(tcph->th_dport));
  }
  if(tcph->th_flags & TH_FIN)
    /* XXX - find a way to know the server port and output only that one */
    expert_add_info_format(pinfo, tf_fin, PI_SEQUENCE, PI_CHAT, "Connection finish (FIN)");
  if(tcph->th_flags & TH_RST)
    /* XXX - find a way to know the server port and output only that one */
    expert_add_info_format(pinfo, tf_rst, PI_SEQUENCE, PI_CHAT, "Connection reset (RST)");

  /* Supply the sequence number of the first byte and of the first byte
     after the segment. */
  tcpinfo.seq = tcph->th_seq;
  tcpinfo.nxtseq = nxtseq;
  tcpinfo.lastackseq = tcph->th_ack;

  /* Assume we'll pass un-reassembled data to subdissectors. */
  tcpinfo.is_reassembled = FALSE;

  pinfo->private_data = &tcpinfo;

  /*
   * Assume, initially, that we can't desegment.
   */
  pinfo->can_desegment = 0;
  th_sum = tvb_get_ntohs(tvb, offset + 16);
  if (!pinfo->fragmented && tvb_bytes_exist(tvb, 0, reported_len)) {
    /* The packet isn't part of an un-reassembled fragmented datagram
       and isn't truncated.  This means we have all the data, and thus
       can checksum it and, unless it's being returned in an error
       packet, are willing to allow subdissectors to request reassembly
       on it. */

    if (tcp_check_checksum) {
      /* We haven't turned checksum checking off; checksum it. */

      /* Set up the fields of the pseudo-header. */
      cksum_vec[0].ptr = pinfo->src.data;
      cksum_vec[0].len = pinfo->src.len;
      cksum_vec[1].ptr = pinfo->dst.data;
      cksum_vec[1].len = pinfo->dst.len;
      cksum_vec[2].ptr = (const guint8 *)phdr;
      switch (pinfo->src.type) {

      case AT_IPv4:
        phdr[0] = g_htonl((IP_PROTO_TCP<<16) + reported_len);
        cksum_vec[2].len = 4;
        break;

      case AT_IPv6:
        phdr[0] = g_htonl(reported_len);
        phdr[1] = g_htonl(IP_PROTO_TCP);
        cksum_vec[2].len = 8;
        break;

      default:
        /* TCP runs only atop IPv4 and IPv6.... */
        DISSECTOR_ASSERT_NOT_REACHED();
        break;
      }
      cksum_vec[3].ptr = tvb_get_ptr(tvb, offset, reported_len);
      cksum_vec[3].len = reported_len;
      computed_cksum = in_cksum(cksum_vec, 4);
      if (computed_cksum == 0 && th_sum == 0xffff) {
        item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
           offset + 16, 2, th_sum,
	   "Checksum: 0x%04x [should be 0x0000 (see RFC 1624)]", th_sum);

	checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);
        expert_add_info_format(pinfo, item, PI_CHECKSUM, PI_WARN, "TCP Checksum 0xffff instead of 0x0000 (see RFC 1624)");

        if (check_col(pinfo->cinfo, COL_INFO))
          col_append_str(pinfo->cinfo, COL_INFO, " [TCP CHECKSUM 0xFFFF]");

        /* Checksum is treated as valid on most systems, so we're willing to desegment it. */
        desegment_ok = TRUE;
      } else if (computed_cksum == 0) {
        item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
          offset + 16, 2, th_sum, "Checksum: 0x%04x [correct]", th_sum);

	checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
	   offset + 16, 2, TRUE);
        PROTO_ITEM_SET_GENERATED(item);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);

        /* Checksum is valid, so we're willing to desegment it. */
        desegment_ok = TRUE;
      } else if (th_sum == 0) {
        /* checksum is probably fine but checksum offload is used */
        item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
          offset + 16, 2, th_sum, "Checksum: 0x%04x [Checksum Offloaded]", th_sum);

	checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);

        /* Checksum is (probably) valid, so we're willing to desegment it. */
        desegment_ok = TRUE;
      } else {
        item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
           offset + 16, 2, th_sum,
	   "Checksum: 0x%04x [incorrect, should be 0x%04x (maybe caused by \"TCP checksum offload\"?)]", th_sum,
	   in_cksum_shouldbe(th_sum, computed_cksum));

	checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
	   offset + 16, 2, TRUE);
        PROTO_ITEM_SET_GENERATED(item);
        expert_add_info_format(pinfo, item, PI_CHECKSUM, PI_ERROR, "Bad checksum");

        if (check_col(pinfo->cinfo, COL_INFO))
          col_append_str(pinfo->cinfo, COL_INFO, " [TCP CHECKSUM INCORRECT]");

        /* Checksum is invalid, so we're not willing to desegment it. */
        desegment_ok = FALSE;
        pinfo->noreassembly_reason = " [incorrect TCP checksum]";
      }
    } else {
        item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
         offset + 16, 2, th_sum, "Checksum: 0x%04x [validation disabled]", th_sum);

	checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);
        item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
	   offset + 16, 2, FALSE);
        PROTO_ITEM_SET_GENERATED(item);

      /* We didn't check the checksum, and don't care if it's valid,
         so we're willing to desegment it. */
      desegment_ok = TRUE;
    }
  } else {
    /* We don't have all the packet data, so we can't checksum it... */
    item = proto_tree_add_uint_format(tcp_tree, hf_tcp_checksum, tvb,
       offset + 16, 2, th_sum, "Checksum: 0x%04x [unchecked, not all data available]", th_sum);

    checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
    item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_good, tvb,
       offset + 16, 2, FALSE);
    PROTO_ITEM_SET_GENERATED(item);
    item = proto_tree_add_boolean(checksum_tree, hf_tcp_checksum_bad, tvb,
       offset + 16, 2, FALSE);
    PROTO_ITEM_SET_GENERATED(item);

    /* ...and aren't willing to desegment it. */
    desegment_ok = FALSE;
  }

  if (desegment_ok) {
    /* We're willing to desegment this.  Is desegmentation enabled? */
    if (tcp_desegment) {
      /* Yes - is this segment being returned in an error packet? */
      if (!pinfo->in_error_pkt) {
		/* No - indicate that we will desegment.
		   We do NOT want to desegment segments returned in error
		   packets, as they're not part of a TCP connection. */
		pinfo->can_desegment = 2;
      }
    }
  }

  if (tcph->th_flags & TH_URG) {
    th_urp = tvb_get_ntohs(tvb, offset + 18);
    /* Export the urgent pointer, for the benefit of protocols such as
       rlogin. */
    tcpinfo.urgent = TRUE;
    tcpinfo.urgent_pointer = th_urp;
    if (check_col(pinfo->cinfo, COL_INFO))
      col_append_fstr(pinfo->cinfo, COL_INFO, " Urg=%u", th_urp);
    if (tcp_tree != NULL)
      proto_tree_add_uint(tcp_tree, hf_tcp_urgent_pointer, tvb, offset + 18, 2, th_urp);
  } else
    tcpinfo.urgent = FALSE;

  if (tcph->th_have_seglen) {
    if (check_col(pinfo->cinfo, COL_INFO))
      col_append_fstr(pinfo->cinfo, COL_INFO, " Len=%u", tcph->th_seglen);
  }

  /* Decode TCP options, if any. */
  if (tcph->th_hlen > TCPH_MIN_LEN) {
    /* There's more than just the fixed-length header.  Decode the
       options. */
    optlen = tcph->th_hlen - TCPH_MIN_LEN; /* length of options, in bytes */
    tvb_ensure_bytes_exist(tvb, offset +  20, optlen);
    if (tcp_tree != NULL) {
      guint8 *p_options = ep_tvb_memdup(tvb, offset + 20, optlen);
      tf = proto_tree_add_bytes_format(tcp_tree, hf_tcp_options, tvb, offset +  20,
        optlen, p_options, "Options: (%u bytes)", optlen);
      field_tree = proto_item_add_subtree(tf, ett_tcp_options);
    } else
      field_tree = NULL;
    dissect_ip_tcp_options(tvb, offset + 20, optlen,
      tcpopts, N_TCP_OPTS, TCPOPT_EOL, pinfo, field_tree);
  }

  if(!pinfo->fd->flags.visited){
    if((tcph->th_flags & (TH_SYN|TH_ACK))==(TH_SYN|TH_ACK)) {
      /* If there was window scaling in the SYN packet but none in the SYN+ACK
       * then we should just forget about the windowscaling completely.
       */
      if(tcp_analyze_seq && tcp_relative_seq){
		verify_tcp_window_scaling(tcpd);
      }
      /* If the SYN or the SYN+ACK offered SCPS capabilities,
       * validate the flow's bidirectional scps capabilities.
       * The or protects against broken implementations offering
       * SCPS capabilities on SYN+ACK even if it wasn't offered with the SYN
       */
      if(tcpd && ((tcpd->rev->scps_capable) || (tcpd->fwd->scps_capable))) {
		verify_scps(pinfo, tf_syn, tcpd);
      }
    }
  }

  /* Skip over header + options */
  offset += tcph->th_hlen;

  /* Check the packet length to see if there's more data
     (it could be an ACK-only packet) */
  length_remaining = tvb_length_remaining(tvb, offset);

  if (tcph->th_have_seglen) {
    if( data_out_file ) {
      reassemble_tcp( tcph->th_seq,             /* sequence number */
          tcph->th_ack,                         /* acknowledgement number */
          tcph->th_seglen,                      /* data length */
          (gchar*)tvb_get_ptr(tvb, offset, length_remaining),	/* data */
          length_remaining,                     /* captured data length */
          ( tcph->th_flags & TH_SYN ),          /* is syn set? */
          &pinfo->net_src,
          &pinfo->net_dst,
          pinfo->srcport,
          pinfo->destport);
    }
  }

  /* handle TCP seq# analysis, print any extra SEQ/ACK data for this segment*/
  if(tcp_analyze_seq){
      tcp_print_sequence_number_analysis(pinfo, tvb, tcp_tree, tcpd);
  }

  /* handle conversation timestamps */
  if(tcp_calculate_ts){
      tcp_print_timestamps(pinfo, tvb, tcp_tree, tcpd, tcppd);
  }

  tap_queue_packet(tcp_tap, pinfo, tcph);


  /* A FIN packet might complete reassembly so we need to explicitly
   * check for this here.
   */
  if(tcpd && (tcph->th_flags & TH_FIN)
      && (tcpd->fwd->flags&TCP_FLOW_REASSEMBLE_UNTIL_FIN) ){
    struct tcp_multisegment_pdu *msp;

    /* find the most previous PDU starting before this sequence number */
    msp=se_tree_lookup32_le(tcpd->fwd->multisegment_pdus, tcph->th_seq-1);
    if(msp){
      fragment_data *ipfd_head;

      ipfd_head = fragment_add(tvb, offset, pinfo, msp->first_frame,
			tcp_fragment_table,
			tcph->th_seq - msp->seq,
			tcph->th_seglen,
			FALSE );
      if(ipfd_head){
        tvbuff_t *next_tvb;

        /* create a new TVB structure for desegmented data
         * datalen-1 to strip the dummy FIN byte off
         */
        next_tvb = tvb_new_child_real_data(tvb, ipfd_head->data, ipfd_head->datalen, ipfd_head->datalen);

        /* add desegmented data to the data source list */
        add_new_data_source(pinfo, next_tvb, "Reassembled TCP");

        /* call the payload dissector
         * but make sure we don't offer desegmentation any more
         */
        pinfo->can_desegment = 0;

        process_tcp_payload(next_tvb, 0, pinfo, tree, tcp_tree, tcph->th_sport, tcph->th_dport, tcph->th_seq, nxtseq, FALSE, tcpd);

        print_tcp_fragment_tree(ipfd_head, tree, tcp_tree, pinfo, next_tvb);

        return;
      }
    }
  }

  if (tcpd && (tcpd->fwd || tcpd->rev) && (tcpd->fwd->command || tcpd->rev->command)) {
    ti = proto_tree_add_text(tcp_tree, tvb, offset, 0, "Process Information");
	PROTO_ITEM_SET_GENERATED(ti);
    field_tree = proto_item_add_subtree(ti, ett_tcp_process_info);
	if (tcpd->fwd->command) {
      proto_tree_add_uint_format_value(field_tree, hf_tcp_proc_dst_uid, tvb, 0, 0,
              tcpd->fwd->process_uid, "%u", tcpd->fwd->process_uid);
      proto_tree_add_uint_format_value(field_tree, hf_tcp_proc_dst_pid, tvb, 0, 0,
              tcpd->fwd->process_pid, "%u", tcpd->fwd->process_pid);
      proto_tree_add_string_format_value(field_tree, hf_tcp_proc_dst_uname, tvb, 0, 0,
              tcpd->fwd->username, "%s", tcpd->fwd->username);
      proto_tree_add_string_format_value(field_tree, hf_tcp_proc_dst_cmd, tvb, 0, 0,
              tcpd->fwd->command, "%s", tcpd->fwd->command);
	}
	if (tcpd->rev->command) {
      proto_tree_add_uint_format_value(field_tree, hf_tcp_proc_src_uid, tvb, 0, 0,
              tcpd->rev->process_uid, "%u", tcpd->rev->process_uid);
      proto_tree_add_uint_format_value(field_tree, hf_tcp_proc_src_pid, tvb, 0, 0,
              tcpd->rev->process_pid, "%u", tcpd->rev->process_pid);
      proto_tree_add_string_format_value(field_tree, hf_tcp_proc_src_uname, tvb, 0, 0,
              tcpd->rev->username, "%s", tcpd->rev->username);
      proto_tree_add_string_format_value(field_tree, hf_tcp_proc_src_cmd, tvb, 0, 0,
              tcpd->rev->command, "%s", tcpd->rev->command);
	}
  }

  /*
   * XXX - what, if any, of this should we do if this is included in an
   * error packet?  It might be nice to see the details of the packet
   * that caused the ICMP error, but it might not be nice to have the
   * dissector update state based on it.
   * Also, we probably don't want to run TCP taps on those packets.
   */
  if (length_remaining != 0) {
    if (tcph->th_flags & TH_RST) {
      /*
       * RFC1122 says:
       *
       *	4.2.2.12  RST Segment: RFC-793 Section 3.4
       *
       *	  A TCP SHOULD allow a received RST segment to include data.
       *
       *	  DISCUSSION
       * 	       It has been suggested that a RST segment could contain
       * 	       ASCII text that encoded and explained the cause of the
       *	       RST.  No standard has yet been established for such
       *	       data.
       *
       * so for segments with RST we just display the data as text.
       */
      proto_tree_add_text(tcp_tree, tvb, offset, length_remaining,
			    "Reset cause: %s",
			    tvb_format_text(tvb, offset, length_remaining));
    } else {
      dissect_tcp_payload(tvb, pinfo, offset, tcph->th_seq, nxtseq,
                          tcph->th_sport, tcph->th_dport, tree, tcp_tree, tcpd);
    }
  }
}

void
proto_register_tcp(void)
{
	static hf_register_info hf[] = {

		{ &hf_tcp_srcport,
		{ "Source Port",		"tcp.srcport", FT_UINT16, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_dstport,
		{ "Destination Port",		"tcp.dstport", FT_UINT16, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_port,
		{ "Source or Destination Port",	"tcp.port", FT_UINT16, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_stream,
		{ "Stream index",		"tcp.stream", FT_UINT32, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_seq,
		{ "Sequence number",		"tcp.seq", FT_UINT32, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_nxtseq,
		{ "Next sequence number",	"tcp.nxtseq", FT_UINT32, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_ack,
		{ "Acknowledgement number",	"tcp.ack", FT_UINT32, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_hdr_len,
		{ "Header Length",		"tcp.hdr_len", FT_UINT8, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_flags,
		{ "Flags",			"tcp.flags", FT_UINT8, BASE_HEX, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_flags_cwr,
		{ "Congestion Window Reduced (CWR)",			"tcp.flags.cwr", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_CWR,
			NULL, HFILL }},

		{ &hf_tcp_flags_ecn,
		{ "ECN-Echo",			"tcp.flags.ecn", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_ECN,
			NULL, HFILL }},

		{ &hf_tcp_flags_urg,
		{ "Urgent",			"tcp.flags.urg", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_URG,
			NULL, HFILL }},

		{ &hf_tcp_flags_ack,
		{ "Acknowledgement",		"tcp.flags.ack", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_ACK,
			NULL, HFILL }},

		{ &hf_tcp_flags_push,
		{ "Push",			"tcp.flags.push", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_PUSH,
			NULL, HFILL }},

		{ &hf_tcp_flags_reset,
		{ "Reset",			"tcp.flags.reset", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_RST,
			NULL, HFILL }},

		{ &hf_tcp_flags_syn,
		{ "Syn",			"tcp.flags.syn", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_SYN,
			NULL, HFILL }},

		{ &hf_tcp_flags_fin,
		{ "Fin",			"tcp.flags.fin", FT_BOOLEAN, 8, TFS(&tfs_set_notset), TH_FIN,
			NULL, HFILL }},

		/* 32 bits so we can present some values adjusted to window scaling */
		{ &hf_tcp_window_size,
		{ "Window size",		"tcp.window_size", FT_UINT32, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_checksum,
		{ "Checksum",			"tcp.checksum", FT_UINT16, BASE_HEX, NULL, 0x0,
			"Details at: http://www.wireshark.org/docs/wsug_html_chunked/ChAdvChecksums.html", HFILL }},

		{ &hf_tcp_checksum_good,
		{ "Good Checksum",		"tcp.checksum_good", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"True: checksum matches packet content; False: doesn't match content or not checked", HFILL }},

		{ &hf_tcp_checksum_bad,
		{ "Bad Checksum",		"tcp.checksum_bad", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"True: checksum doesn't match packet content; False: matches content or not checked", HFILL }},

		{ &hf_tcp_analysis_flags,
		{ "TCP Analysis Flags",		"tcp.analysis.flags", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame has some of the TCP analysis flags set", HFILL }},

		{ &hf_tcp_analysis_retransmission,
		{ "Retransmission",		"tcp.analysis.retransmission", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame is a suspected TCP retransmission", HFILL }},

		{ &hf_tcp_analysis_fast_retransmission,
		{ "Fast Retransmission",		"tcp.analysis.fast_retransmission", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame is a suspected TCP fast retransmission", HFILL }},

		{ &hf_tcp_analysis_out_of_order,
		{ "Out Of Order",		"tcp.analysis.out_of_order", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame is a suspected Out-Of-Order segment", HFILL }},

		{ &hf_tcp_analysis_reused_ports,
		{ "TCP Port numbers reused",		"tcp.analysis.reused_ports", FT_NONE, BASE_NONE, NULL, 0x0,
			"A new tcp session has started with previously used port numbers", HFILL }},

		{ &hf_tcp_analysis_lost_packet,
		{ "Previous Segment Lost",		"tcp.analysis.lost_segment", FT_NONE, BASE_NONE, NULL, 0x0,
			"A segment before this one was lost from the capture", HFILL }},

		{ &hf_tcp_analysis_ack_lost_packet,
		{ "ACKed Lost Packet",		"tcp.analysis.ack_lost_segment", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame ACKs a lost segment", HFILL }},

		{ &hf_tcp_analysis_window_update,
		{ "Window update",		"tcp.analysis.window_update", FT_NONE, BASE_NONE, NULL, 0x0,
			"This frame is a tcp window update", HFILL }},

		{ &hf_tcp_analysis_window_full,
		{ "Window full",		"tcp.analysis.window_full", FT_NONE, BASE_NONE, NULL, 0x0,
			"This segment has caused the allowed window to become 100% full", HFILL }},

		{ &hf_tcp_analysis_keep_alive,
		{ "Keep Alive",		"tcp.analysis.keep_alive", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is a keep-alive segment", HFILL }},

		{ &hf_tcp_analysis_keep_alive_ack,
		{ "Keep Alive ACK",		"tcp.analysis.keep_alive_ack", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is an ACK to a keep-alive segment", HFILL }},

		{ &hf_tcp_analysis_duplicate_ack,
		{ "Duplicate ACK",		"tcp.analysis.duplicate_ack", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is a duplicate ACK", HFILL }},

		{ &hf_tcp_analysis_duplicate_ack_num,
		{ "Duplicate ACK #",		"tcp.analysis.duplicate_ack_num", FT_UINT32, BASE_DEC, NULL, 0x0,
			"This is duplicate ACK number #", HFILL }},

		{ &hf_tcp_analysis_duplicate_ack_frame,
		{ "Duplicate to the ACK in frame",		"tcp.analysis.duplicate_ack_frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"This is a duplicate to the ACK in frame #", HFILL }},

		{ &hf_tcp_continuation_to,
		{ "This is a continuation to the PDU in frame",		"tcp.continuation_to", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"This is a continuation to the PDU in frame #", HFILL }},

		{ &hf_tcp_analysis_zero_window_probe,
		{ "Zero Window Probe",		"tcp.analysis.zero_window_probe", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is a zero-window-probe", HFILL }},

		{ &hf_tcp_analysis_zero_window_probe_ack,
		{ "Zero Window Probe Ack",		"tcp.analysis.zero_window_probe_ack", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is an ACK to a zero-window-probe", HFILL }},

		{ &hf_tcp_analysis_zero_window,
		{ "Zero Window",		"tcp.analysis.zero_window", FT_NONE, BASE_NONE, NULL, 0x0,
			"This is a zero-window", HFILL }},

		{ &hf_tcp_len,
		  { "TCP Segment Len",            "tcp.len", FT_UINT32, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_analysis_acks_frame,
		  { "This is an ACK to the segment in frame",            "tcp.analysis.acks_frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
		    "Which previous segment is this an ACK for", HFILL}},

		{ &hf_tcp_analysis_bytes_in_flight,
		  { "Number of bytes in flight",            "tcp.analysis.bytes_in_flight", FT_UINT32, BASE_DEC, NULL, 0x0,
		    "How many bytes are now in flight for this connection", HFILL}},

		{ &hf_tcp_analysis_ack_rtt,
		  { "The RTT to ACK the segment was",            "tcp.analysis.ack_rtt", FT_RELATIVE_TIME, BASE_NONE, NULL, 0x0,
		    "How long time it took to ACK the segment (RTT)", HFILL}},

		{ &hf_tcp_analysis_rto,
		  { "The RTO for this segment was",            "tcp.analysis.rto", FT_RELATIVE_TIME, BASE_NONE, NULL, 0x0,
		    "How long transmission was delayed before this segment was retransmitted (RTO)", HFILL}},

		{ &hf_tcp_analysis_rto_frame,
		  { "RTO based on delta from frame", "tcp.analysis.rto_frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"This is the frame we measure the RTO from", HFILL }},

		{ &hf_tcp_urgent_pointer,
		{ "Urgent pointer",		"tcp.urgent_pointer", FT_UINT16, BASE_DEC, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_segment_overlap,
		{ "Segment overlap",	"tcp.segment.overlap", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"Segment overlaps with other segments", HFILL }},

		{ &hf_tcp_segment_overlap_conflict,
		{ "Conflicting data in segment overlap",	"tcp.segment.overlap.conflict", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"Overlapping segments contained conflicting data", HFILL }},

		{ &hf_tcp_segment_multiple_tails,
		{ "Multiple tail segments found",	"tcp.segment.multipletails", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"Several tails were found when reassembling the pdu", HFILL }},

		{ &hf_tcp_segment_too_long_fragment,
		{ "Segment too long",	"tcp.segment.toolongfragment", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			"Segment contained data past end of the pdu", HFILL }},

		{ &hf_tcp_segment_error,
		{ "Reassembling error", "tcp.segment.error", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"Reassembling error due to illegal segments", HFILL }},

		{ &hf_tcp_segment,
		{ "TCP Segment", "tcp.segment", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			NULL, HFILL }},

		{ &hf_tcp_segments,
		{ "Reassembled TCP Segments", "tcp.segments", FT_NONE, BASE_NONE, NULL, 0x0,
			"TCP Segments", HFILL }},

		{ &hf_tcp_reassembled_in,
		{ "Reassembled PDU in frame", "tcp.reassembled_in", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"The PDU that doesn't end in this segment is reassembled in this frame", HFILL }},

		{ &hf_tcp_options,
		  { "TCP Options", "tcp.options", FT_BYTES,
		    BASE_NONE, NULL, 0x0, NULL, HFILL }},

		{ &hf_tcp_option_mss,
		  { "TCP MSS Option", "tcp.options.mss", FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL }},

		{ &hf_tcp_option_mss_val,
		  { "TCP MSS Option Value", "tcp.options.mss_val", FT_UINT16,
		    BASE_DEC, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_wscale,
		  { "TCP Window Scale Option", "tcp.options.wscale",
		    FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, "TCP Window Option", HFILL}},

		{ &hf_tcp_option_wscale_val,
		  { "TCP Windows Scale Option Value", "tcp.options.wscale_val",
		    FT_UINT8, BASE_DEC, NULL, 0x0, "TCP Window Scale Value",
		    HFILL}},

		{ &hf_tcp_option_sack_perm,
		  { "TCP Sack Perm Option", "tcp.options.sack_perm",
		    FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_sack,
		  { "TCP Sack Option", "tcp.options.sack", FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_sack_sle,
		  {"TCP Sack Left Edge", "tcp.options.sack_le", FT_UINT32,
		   BASE_DEC, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_sack_sre,
		  {"TCP Sack Right Edge", "tcp.options.sack_re", FT_UINT32,
		   BASE_DEC, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_echo,
		  { "TCP Echo Option", "tcp.options.echo", FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, "TCP Sack Echo", HFILL}},

		{ &hf_tcp_option_echo_reply,
		  { "TCP Echo Reply Option", "tcp.options.echo_reply",
		    FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_time_stamp,
		  { "TCP Time Stamp Option", "tcp.options.time_stamp",
		    FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_cc,
		  { "TCP CC Option", "tcp.options.cc", FT_BOOLEAN, BASE_NONE,
		    NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_ccnew,
		  { "TCP CC New Option", "tcp.options.ccnew", FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_ccecho,
		  { "TCP CC Echo Option", "tcp.options.ccecho", FT_BOOLEAN,
		    BASE_NONE, NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_md5,
		  { "TCP MD5 Option", "tcp.options.md5", FT_BOOLEAN, BASE_NONE,
		    NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_qs,
		  { "TCP QS Option", "tcp.options.qs", FT_BOOLEAN, BASE_NONE,
		    NULL, 0x0, NULL, HFILL}},

		{ &hf_tcp_option_scps,
		  { "TCP SCPS Capabilities Option", "tcp.options.scps",
		    FT_BOOLEAN, BASE_NONE, NULL,  0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_scps_vector,
		  { "TCP SCPS Capabilities Vector", "tcp.options.scps.vector",
		    FT_UINT8, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_scps_binding,
		  { "TCP SCPS Extended Binding Spacce",
		    "tcp.options.scps.binding",
		    FT_UINT8, BASE_DEC, NULL, 0x0,
		    "TCP SCPS Extended Binding Space", HFILL}},

		{ &hf_tcp_option_snack,
		  { "TCP Selective Negative Acknowledgement Option",
		    "tcp.options.snack",
		    FT_BOOLEAN, BASE_NONE, NULL,  0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_snack_offset,
		  { "TCP SNACK Offset", "tcp.options.snack.offset",
		    FT_UINT16, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_snack_size,
		  { "TCP SNACK Size", "tcp.options.snack.size",
		    FT_UINT16, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_snack_le,
		  { "TCP SNACK Left Edge", "tcp.options.snack.le",
		    FT_UINT16, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_option_snack_re,
		  { "TCP SNACK Right Edge", "tcp.options.snack.re",
		    FT_UINT16, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_scpsoption_flags_bets,
		  { "Partial Reliability Capable (BETS)",
		    "tcp.options.scpsflags.bets", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x80, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_snack1,
		  { "Short Form SNACK Capable (SNACK1)",
		    "tcp.options.scpsflags.snack1", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x40, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_snack2,
		  { "Long Form SNACK Capable (SNACK2)",
		    "tcp.options.scpsflags.snack2", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x20, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_compress,
		  { "Lossless Header Compression (COMP)",
		    "tcp.options.scpsflags.compress", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x10, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_nlts,
		  { "Network Layer Timestamp (NLTS)",
		    "tcp.options.scpsflags.nlts", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x8, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_resv1,
		  { "Reserved Bit 1",
		    "tcp.options.scpsflags.reserved1", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x4, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_resv2,
		  { "Reserved Bit 2",
		    "tcp.options.scpsflags.reserved2", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x2, NULL, HFILL }},

		{ &hf_tcp_scpsoption_flags_resv3,
		  { "Reserved Bit 3",
		    "tcp.options.scpsflags.reserved3", FT_BOOLEAN, 8,
		    TFS(&tfs_set_notset), 0x1, NULL, HFILL }},

		{ &hf_tcp_pdu_time,
		  { "Time until the last segment of this PDU", "tcp.pdu.time", FT_RELATIVE_TIME, BASE_NONE, NULL, 0x0,
		    "How long time has passed until the last frame of this PDU", HFILL}},

		{ &hf_tcp_pdu_size,
		  { "PDU Size", "tcp.pdu.size", FT_UINT32, BASE_DEC, NULL, 0x0,
		    "The size of this PDU", HFILL}},

		{ &hf_tcp_pdu_last_frame,
		  { "Last frame of this PDU", "tcp.pdu.last_frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			"This is the last frame of the PDU starting in this segment", HFILL }},

		{ &hf_tcp_ts_relative,
		  { "Time since first frame in this TCP stream", "tcp.time_relative", FT_RELATIVE_TIME, BASE_NONE, NULL, 0x0,
		    "Time relative to first frame in this TCP stream", HFILL}},

		{ &hf_tcp_ts_delta,
		  { "Time since previous frame in this TCP stream", "tcp.time_delta", FT_RELATIVE_TIME, BASE_NONE, NULL, 0x0,
		    "Time delta from previous frame in this TCP stream", HFILL}},

		{ &hf_tcp_proc_src_uid,
		  { "Source process user ID", "tcp.proc.srcuid", FT_UINT32, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_src_pid,
		  { "Source process ID", "tcp.proc.srcpid", FT_UINT32, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_src_uname,
		  { "Source process user name", "tcp.proc.srcuname", FT_STRING, BASE_NONE, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_src_cmd,
		  { "Source process name", "tcp.proc.srccmd", FT_STRING, BASE_NONE, NULL, 0x0,
		    "Source process command name", HFILL}},

		{ &hf_tcp_proc_dst_uid,
		  { "Destination process user ID", "tcp.proc.dstuid", FT_UINT32, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_dst_pid,
		  { "Destination process ID", "tcp.proc.dstpid", FT_UINT32, BASE_DEC, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_dst_uname,
		  { "Destination process user name", "tcp.proc.dstuname", FT_STRING, BASE_NONE, NULL, 0x0,
		    NULL, HFILL}},

		{ &hf_tcp_proc_dst_cmd,
		  { "Destination process name", "tcp.proc.dstcmd", FT_STRING, BASE_NONE, NULL, 0x0,
		    "Destination process command name", HFILL}}
	};

	static gint *ett[] = {
		&ett_tcp,
		&ett_tcp_flags,
		&ett_tcp_options,
		&ett_tcp_option_sack,
		&ett_tcp_option_scps,
		&ett_tcp_option_scps_extended,
		&ett_tcp_analysis_faults,
		&ett_tcp_analysis,
		&ett_tcp_timestamps,
		&ett_tcp_segments,
		&ett_tcp_segment,
		&ett_tcp_checksum,
		&ett_tcp_process_info
	};
	module_t *tcp_module;

	proto_tcp = proto_register_protocol("Transmission Control Protocol",
	    "TCP", "tcp");
	proto_register_field_array(proto_tcp, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	/* subdissector code */
	subdissector_table = register_dissector_table("tcp.port",
	    "TCP port", FT_UINT16, BASE_DEC);
	register_heur_dissector_list("tcp", &heur_subdissector_list);

	/* Register configuration preferences */
	tcp_module = prefs_register_protocol(proto_tcp, NULL);
	prefs_register_bool_preference(tcp_module, "summary_in_tree",
	    "Show TCP summary in protocol tree",
	    "Whether the TCP summary line should be shown in the protocol tree",
	    &tcp_summary_in_tree);
	prefs_register_bool_preference(tcp_module, "check_checksum",
	    "Validate the TCP checksum if possible",
	    "Whether to validate the TCP checksum",
	    &tcp_check_checksum);
	prefs_register_bool_preference(tcp_module, "desegment_tcp_streams",
	    "Allow subdissector to reassemble TCP streams",
	    "Whether subdissector can request TCP streams to be reassembled",
	    &tcp_desegment);
	prefs_register_bool_preference(tcp_module, "analyze_sequence_numbers",
	    "Analyze TCP sequence numbers",
	    "Make the TCP dissector analyze TCP sequence numbers to find and flag segment retransmissions, missing segments and RTT",
	    &tcp_analyze_seq);
	prefs_register_bool_preference(tcp_module, "relative_sequence_numbers",
	    "Relative sequence numbers and window scaling",
	    "Make the TCP dissector use relative sequence numbers instead of absolute ones. "
	    "To use this option you must also enable \"Analyze TCP sequence numbers\". "
	    "This option will also try to track and adjust the window field according to any TCP window scaling options seen.",
	    &tcp_relative_seq);
	prefs_register_bool_preference(tcp_module, "track_bytes_in_flight",
	    "Track number of bytes in flight",
	    "Make the TCP dissector track the number on un-ACKed bytes of data are in flight per packet. "
	    "To use this option you must also enable \"Analyze TCP sequence numbers\". "
	    "This takes a lot of memory but allows you to track how much data are in flight at a time and graphing it in io-graphs",
	    &tcp_track_bytes_in_flight);
	prefs_register_bool_preference(tcp_module, "calculate_timestamps",
	    "Calculate conversation timestamps",
	    "Calculate timestamps relative to the first frame and the previous frame in the tcp conversation",
	    &tcp_calculate_ts);
	prefs_register_bool_preference(tcp_module, "try_heuristic_first",
	    "Try heuristic sub-dissectors first",
	    "Try to decode a packet using an heuristic sub-dissector before using a sub-dissector registered to a specific port",
	    &try_heuristic_first);

	register_init_routine(tcp_fragment_init);
}

void
proto_reg_handoff_tcp(void)
{
	dissector_handle_t tcp_handle;

	tcp_handle = create_dissector_handle(dissect_tcp, proto_tcp);
	dissector_add("ip.proto", IP_PROTO_TCP, tcp_handle);
	data_handle = find_dissector("data");
	tcp_tap = register_tap("tcp");
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set shiftwidth=4 tabstop=4 noexpandtab
 * :indentSize=4:tabSize=4:noTabs=false:
 */
