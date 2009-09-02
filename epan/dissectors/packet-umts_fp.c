/* Routines for UMTS FP disassembly
 *
 * Martin Mathieson
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

#include <epan/packet.h>
#include <epan/expert.h>

#include "packet-umts_fp.h"

/* The Frame Protocol (FP) is described in:
 * 3GPP TS 25.427 (for dedicated channels)
 * 3GPP TS 25.435 (for common/shared channels)
 *
 * TODO:
 *  - IUR interface-specific formats
 *  - verify header & payload CRCs
 */

/* Initialize the protocol and registered fields. */
int proto_fp = -1;

static int hf_fp_channel_type = -1;
static int hf_fp_division = -1;
static int hf_fp_direction = -1;
static int hf_fp_header_crc = -1;
static int hf_fp_ft = -1;
static int hf_fp_cfn = -1;
static int hf_fp_pch_cfn = -1;
static int hf_fp_pch_toa = -1;
static int hf_fp_cfn_control = -1;
static int hf_fp_toa = -1;
static int hf_fp_tfi = -1;
static int hf_fp_usch_tfi = -1;
static int hf_fp_cpch_tfi = -1;
static int hf_fp_propagation_delay = -1;
static int hf_fp_tb = -1;
static int hf_fp_chan_zero_tbs = -1;
static int hf_fp_received_sync_ul_timing_deviation = -1;
static int hf_fp_pch_pi = -1;
static int hf_fp_pch_tfi = -1;
static int hf_fp_fach_tfi = -1;
static int hf_fp_transmit_power_level = -1;
static int hf_fp_paging_indication_bitmap = -1;
static int hf_fp_pdsch_set_id = -1;
static int hf_fp_rx_timing_deviation = -1;
static int hf_fp_dch_e_rucch_flag = -1;
static int hf_fp_dch_control_frame_type = -1;
static int hf_fp_dch_rx_timing_deviation = -1;
static int hf_fp_quality_estimate = -1;
static int hf_fp_payload_crc = -1;
static int hf_fp_edch_header_crc = -1;
static int hf_fp_edch_fsn = -1;
static int hf_fp_edch_subframe = -1;
static int hf_fp_edch_number_of_subframes = -1;
static int hf_fp_edch_harq_retransmissions = -1;
static int hf_fp_edch_subframe_number = -1;
static int hf_fp_edch_number_of_mac_es_pdus = -1;
static int hf_fp_edch_ddi = -1;
static int hf_fp_edch_subframe_header = -1;
static int hf_fp_edch_number_of_mac_d_pdus = -1;
static int hf_fp_edch_pdu_padding = -1;
static int hf_fp_edch_tsn = -1;
static int hf_fp_edch_mac_es_pdu = -1;
static int hf_fp_frame_seq_nr = -1;
static int hf_fp_hsdsch_pdu_block_header = -1;
static int hf_fp_hsdsch_pdu_block = -1;
static int hf_fp_flush = -1;
static int hf_fp_fsn_drt_reset = -1;
static int hf_fp_drt_indicator = -1;
static int hf_fp_fach_indicator = -1;
static int hf_fp_total_pdu_blocks = -1;
static int hf_fp_drt = -1;
static int hf_fp_hrnti = -1;
static int hf_fp_rach_measurement_result = -1;
static int hf_fp_lchid = -1;
static int hf_fp_pdu_length_in_block = -1;
static int hf_fp_pdus_in_block = -1;
static int hf_fp_cmch_pi = -1;
static int hf_fp_user_buffer_size = -1;
static int hf_fp_hsdsch_credits = -1;
static int hf_fp_hsdsch_max_macd_pdu_len = -1;
static int hf_fp_hsdsch_max_macdc_pdu_len = -1;
static int hf_fp_hsdsch_interval = -1;
static int hf_fp_hsdsch_calculated_rate = -1;
static int hf_fp_hsdsch_unlimited_rate = -1;
static int hf_fp_hsdsch_repetition_period = -1;
static int hf_fp_hsdsch_data_padding = -1;
static int hf_fp_hsdsch_new_ie_flags = -1;
static int hf_fp_hsdsch_new_ie_flag[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int hf_fp_hsdsch_drt = -1;
static int hf_fp_hsdsch_entity = -1;
static int hf_fp_timing_advance = -1;
static int hf_fp_num_of_pdu = -1;
static int hf_fp_mac_d_pdu_len = -1;
static int hf_fp_mac_d_pdu = -1;
static int hf_fp_data = -1;
static int hf_fp_crcis = -1;
static int hf_fp_crci[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int hf_fp_common_control_frame_type = -1;
static int hf_fp_t1 = -1;
static int hf_fp_t2 = -1;
static int hf_fp_t3 = -1;
static int hf_fp_ul_sir_target = -1;
static int hf_fp_pusch_set_id = -1;
static int hf_fp_activation_cfn = -1;
static int hf_fp_duration = -1;
static int hf_fp_power_offset = -1;
static int hf_fp_code_number = -1;
static int hf_fp_spreading_factor = -1;
static int hf_fp_mc_info = -1;

static int hf_fp_rach_new_ie_flags = -1;
static int hf_fp_rach_new_ie_flag_unused[7] = {-1, -1, -1, -1, -1, -1, -1 };
static int hf_fp_rach_ext_propagation_delay_present = -1;
static int hf_fp_rach_cell_portion_id_present = -1;
static int hf_fp_rach_angle_of_arrival_present = -1;
static int hf_fp_rach_ext_rx_sync_ul_timing_deviation_present = -1;
static int hf_fp_rach_ext_rx_timing_deviation_present = -1;

static int hf_fp_cell_portion_id = -1;
static int hf_fp_ext_propagation_delay = -1;
static int hf_fp_angle_of_arrival = -1;
static int hf_fp_ext_received_sync_ul_timing_deviation = -1;

static int hf_fp_radio_interface_parameter_update_flag[5] = {-1, -1, -1, -1, -1};
static int hf_fp_dpc_mode = -1;
static int hf_fp_tpc_po = -1;
static int hf_fp_multiple_rl_set_indicator = -1;
static int hf_fp_max_ue_tx_pow = -1;
static int hf_fp_congestion_status = -1;
static int hf_fp_e_rucch_present = -1;
static int hf_fp_extended_bits_present = -1;
static int hf_fp_extended_bits = -1;
static int hf_fp_spare_extension = -1;

/* Subtrees. */
static int ett_fp = -1;
static int ett_fp_data = -1;
static int ett_fp_crcis = -1;
static int ett_fp_edch_subframe_header = -1;
static int ett_fp_edch_subframe = -1;
static int ett_fp_hsdsch_new_ie_flags = -1;
static int ett_fp_rach_new_ie_flags = -1;
static int ett_fp_hsdsch_pdu_block_header = -1;


/* E-DCH channel header information */
struct subframe_info
{
    guint8  subframe_number;
    guint8  number_of_mac_es_pdus;
    guint8  ddi[64];
    guint16 number_of_mac_d_pdus[64];
};


static const value_string channel_type_vals[] =
{
    { CHANNEL_RACH_FDD,     "RACH_FDD" },
    { CHANNEL_RACH_TDD,     "RACH_TDD" },
    { CHANNEL_FACH_FDD,     "FACH_FDD" },
    { CHANNEL_FACH_TDD,     "FACH_TDD" },
    { CHANNEL_DSCH_FDD,     "DSCH_FDD" },
    { CHANNEL_DSCH_TDD,     "DSCH_TDD" },
    { CHANNEL_USCH_TDD_384, "USCH_TDD_384" },
    { CHANNEL_USCH_TDD_128, "USCH_TDD_128" },
    { CHANNEL_PCH,          "PCH" },
    { CHANNEL_CPCH,         "CPCH" },
    { CHANNEL_BCH,          "BCH" },
    { CHANNEL_DCH,          "DCH" },
    { CHANNEL_HSDSCH,       "HSDSCH" },
    { CHANNEL_IUR_CPCHF,    "IUR CPCHF" },
    { CHANNEL_IUR_FACH,     "IUR FACH" },
    { CHANNEL_IUR_DSCH,     "IUR DSCH" },
    { CHANNEL_EDCH,         "EDCH" },
    { CHANNEL_RACH_TDD_128, "RACH_TDD_128" },
    { 0, NULL }
};

static const value_string division_vals[] =
{
    { Division_FDD,      "FDD"},
    { Division_TDD_384,  "TDD-384"},
    { Division_TDD_128,  "TDD-128"},
    { Division_TDD_768,  "TDD-768"},
    { 0, NULL }
};


static const value_string data_control_vals[] = {
    { 0,   "Data" },
    { 1,   "Control" },
    { 0,   NULL },
};

static const value_string direction_vals[] = {
    { 0,   "Downlink" },
    { 1,   "Uplink" },
    { 0,   NULL },
};

static const value_string crci_vals[] = {
    { 0,   "Correct" },
    { 1,   "Not correct" },
    { 0,   NULL },
};

static const value_string paging_indication_vals[] = {
    { 0,   "no PI-bitmap in payload" },
    { 1,   "PI-bitmap in payload" },
    { 0,   NULL },
};

static const value_string spreading_factor_vals[] = {
    {0,    "4"},
    {1,    "8"},
    {2,    "16"},
    {3,    "32"},
    {4,    "64"},
    {5,    "128"},
    {6,    "256"},
    {0,    NULL }
};

static const value_string congestion_status_vals[] = {
    {0,    "No TNL congestion"},
    {1,    "Reserved for future use"},
    {2,    "TNL congestion - detected by delay build-up"},
    {3,    "TNL congestion - detected by frame loss"},
    {0,    NULL }
};

static const value_string e_rucch_flag_vals[] = {
    { 0,   "Conventional E-RUCCH reception" },
    { 1,   "TA Request reception" },
    { 0,   NULL }
};

static const value_string hsdshc_mac_entity_vals[] = {
    { entity_not_specified,    "Unspecified (assume MAC-hs)" },
    { hs,                      "MAC-hs" },
    { ehs,                     "MAC-ehs" },
    { 0,   NULL }
};


/* Dedicated control types */
#define DCH_OUTER_LOOP_POWER_CONTROL            1
#define DCH_TIMING_ADJUSTMENT                   2
#define DCH_DL_SYNCHRONISATION                  3
#define DCH_UL_SYNCHRONISATION                  4

#define DCH_DL_NODE_SYNCHRONISATION             6
#define DCH_UL_NODE_SYNCHRONISATION             7
#define DCH_RX_TIMING_DEVIATION                 8
#define DCH_RADIO_INTERFACE_PARAMETER_UPDATE    9
#define DCH_TIMING_ADVANCE                      10
#define DCH_TNL_CONGESTION_INDICATION           11

static const value_string dch_control_frame_type_vals[] = {
    { DCH_OUTER_LOOP_POWER_CONTROL,         "OUTER LOOP POWER CONTROL" },
    { DCH_TIMING_ADJUSTMENT,                "TIMING ADJUSTMENT" },
    { DCH_DL_SYNCHRONISATION,               "DL SYNCHRONISATION" },
    { DCH_UL_SYNCHRONISATION,               "UL SYNCHRONISATION" },
    { 5,                                    "Reserved Value" },
    { DCH_DL_NODE_SYNCHRONISATION,          "DL NODE SYNCHRONISATION" },
    { DCH_UL_NODE_SYNCHRONISATION,          "UL NODE SYNCHRONISATION" },
    { DCH_RX_TIMING_DEVIATION,              "RX TIMING DEVIATION" },
    { DCH_RADIO_INTERFACE_PARAMETER_UPDATE, "RADIO INTERFACE PARAMETER UPDATE" },
    { DCH_TIMING_ADVANCE,                   "TIMING ADVANCE" },
    { DCH_TNL_CONGESTION_INDICATION,        "TNL CONGESTION INDICATION" },
    { 0,   NULL },
};


/* Common channel control types */
#define COMMON_OUTER_LOOP_POWER_CONTROL                1
#define COMMON_TIMING_ADJUSTMENT                       2
#define COMMON_DL_SYNCHRONISATION                      3
#define COMMON_UL_SYNCHRONISATION                      4
                                                       
#define COMMON_DL_NODE_SYNCHRONISATION                 6
#define COMMON_UL_NODE_SYNCHRONISATION                 7
#define COMMON_DYNAMIC_PUSCH_ASSIGNMENT                8
#define COMMON_TIMING_ADVANCE                          9
#define COMMON_HS_DSCH_Capacity_Request                10
#define COMMON_HS_DSCH_Capacity_Allocation             11
#define COMMON_HS_DSCH_Capacity_Allocation_Type_2      12

static const value_string common_control_frame_type_vals[] = {
    { COMMON_OUTER_LOOP_POWER_CONTROL,            "OUTER LOOP POWER CONTROL" },
    { COMMON_TIMING_ADJUSTMENT,                   "TIMING ADJUSTMENT" },
    { COMMON_DL_SYNCHRONISATION,                  "DL SYNCHRONISATION" },
    { COMMON_UL_SYNCHRONISATION,                  "UL SYNCHRONISATION" },
    { 5,                                          "Reserved Value" },
    { COMMON_DL_NODE_SYNCHRONISATION,             "DL NODE SYNCHRONISATION" },
    { COMMON_UL_NODE_SYNCHRONISATION,             "UL NODE SYNCHRONISATION" },
    { COMMON_DYNAMIC_PUSCH_ASSIGNMENT,            "DYNAMIC PUSCH ASSIGNMENT" },
    { COMMON_TIMING_ADVANCE,                      "TIMING ADVANCE" },
    { COMMON_HS_DSCH_Capacity_Request,            "HS-DSCH Capacity Request" },
    { COMMON_HS_DSCH_Capacity_Allocation,         "HS-DSCH Capacity Allocation" },
    { COMMON_HS_DSCH_Capacity_Allocation_Type_2,  "HS-DSCH Capacity Allocation Type 2" },
    { 0,   NULL },
};

/* Dissect message parts */
static int dissect_tb_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                           int offset, struct fp_info *p_fp_info, int *num_tbs);
static int dissect_macd_pdu_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                 int offset, guint16 length, guint16 number_of_pdus);
static int dissect_macd_pdu_data_type_2(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                        int offset, guint16 length, guint16 number_of_pdus);
static int dissect_crci_bits(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                             int num_tbs, int offset);
static void dissect_spare_extension_and_crc(tvbuff_t *tvb, packet_info *pinfo,
                                            proto_tree *tree, guint8 dch_crc_present,
                                            int offset);

/* Dissect common control messages */
static int dissect_common_outer_loop_power_control(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb,
                                                   int offset, struct fp_info *p_fp_info);
static int dissect_common_timing_adjustment(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb,
                                            int offset, struct fp_info *p_fp_info);
static int dissect_common_dl_node_synchronisation(packet_info *pinfo, proto_tree *tree,
                                                  tvbuff_t *tvb, int offset);
static int dissect_common_ul_node_synchronisation(packet_info *pinfo, proto_tree *tree,
                                                  tvbuff_t *tvb, int offset);
static int dissect_common_dl_synchronisation(packet_info *pinfo, proto_tree *tree,
                                            tvbuff_t *tvb, int offset,
                                            struct fp_info *p_fp_info);
static int dissect_common_ul_synchronisation(packet_info *pinfo, proto_tree *tree,
                                            tvbuff_t *tvb, int offset,
                                            struct fp_info *p_fp_info);
static int dissect_common_timing_advance(packet_info *pinfo, proto_tree *tree,
                                         tvbuff_t *tvb, int offset);
static int dissect_hsdpa_capacity_request(packet_info *pinfo, proto_tree *tree,
                                          tvbuff_t *tvb, int offset);
static int dissect_hsdpa_capacity_allocation(packet_info *pinfo, proto_tree *tree,
                                             tvbuff_t *tvb, int offset,
                                             struct fp_info *p_fp_info);
static int dissect_hsdpa_capacity_allocation_type_2(packet_info *pinfo, proto_tree *tree,
                                                    tvbuff_t *tvb, int offset);
static void dissect_common_control(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                   int offset, struct fp_info *p_fp_info);
static int dissect_common_dynamic_pusch_assignment(packet_info *pinfo, proto_tree *tree,
                                                   tvbuff_t *tvb, int offset);

/* Dissect common channel types */
static void dissect_rach_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                      int offset, struct fp_info *p_fp_info);
static void dissect_fach_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                      int offset, struct fp_info *p_fp_info);
static void dissect_dsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                      int offset, struct fp_info *p_fp_info);
static void dissect_usch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                      int offset, struct fp_info *p_fp_info);
static void dissect_pch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                     int offset, struct fp_info *p_fp_info);
static void dissect_cpch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                      int offset, struct fp_info *p_fp_info);
static void dissect_bch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                     int offset, struct fp_info *p_fp_info);
static void dissect_iur_dsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                          int offset, struct fp_info *p_fp_info _U_);
static void dissect_hsdsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                        int offset, struct fp_info *p_fp_info);
static void dissect_hsdsch_type_2_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                               int offset, struct fp_info *p_fp_info);


/* Dissect DCH control messages */
static int dissect_dch_timing_adjustment(proto_tree *tree, packet_info *pinfo,
                                         tvbuff_t *tvb, int offset);
static int dissect_dch_rx_timing_deviation(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb, int offset,
                                           struct fp_info *p_fp_info);
static int dissect_dch_dl_synchronisation(proto_tree *tree, packet_info *pinfo,
                                          tvbuff_t *tvb, int offset);
static int dissect_dch_ul_synchronisation(proto_tree *tree, packet_info *pinfo,
                                          tvbuff_t *tvb, int offset);
static int dissect_dch_outer_loop_power_control(proto_tree *tree, packet_info *pinfo,
                                                tvbuff_t *tvb, int offset);
static int dissect_dch_dl_node_synchronisation(proto_tree *tree, packet_info *pinfo,
                                               tvbuff_t *tvb, int offset);
static int dissect_dch_ul_node_synchronisation(proto_tree *tree, packet_info *pinfo,
                                               tvbuff_t *tvb, int offset);
static int dissect_dch_radio_interface_parameter_update(proto_tree *tree, packet_info *pinfo,
                                                        tvbuff_t *tvb, int offset);
static int dissect_dch_timing_advance(proto_tree *tree, packet_info *pinfo,
                                      tvbuff_t *tvb, int offset, struct fp_info *p_fp_info);
static int dissect_dch_tnl_congestion_indication(proto_tree *tree, packet_info *pinfo,
                                                 tvbuff_t *tvb, int offset);


static void dissect_dch_control_frame(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb,
                                      int offset, struct fp_info *p_fp_info);


/* Dissect a DCH channel */
static void dissect_dch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                     int offset, struct fp_info *p_fp_info);

/* Dissect dedicated channels */
static void dissect_e_dch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                       int offset, struct fp_info *p_fp_info);

/* Main dissection function */
static void dissect_fp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree);

/* Protocol registration */
void proto_register_fp(void);
void proto_reg_handoff_fp(void);




/* Dissect the TBs of a data frame */
int dissect_tb_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                    int offset, struct fp_info *p_fp_info, int *num_tbs)
{
    int chan;
    int bit_offset = 0;
    guint data_bits = 0;
    proto_item *tree_ti = NULL;
    proto_tree *data_tree = NULL;

    if (tree)
    {
        /* Add data subtree */
        tree_ti =  proto_tree_add_item(tree, hf_fp_data, tvb, offset, -1, FALSE);
        proto_item_set_text(tree_ti, "TB data for %u chans", p_fp_info->num_chans);
        data_tree = proto_item_add_subtree(tree_ti, ett_fp_data);
    }

    /* Now for the TB data */
    for (chan=0; chan < p_fp_info->num_chans; chan++)
    {
        int n;

        /* Clearly show channels with no TBs */
        if (p_fp_info->chan_num_tbs[chan] == 0)
        {
            proto_item *no_tb_ti = proto_tree_add_uint(data_tree, hf_fp_chan_zero_tbs, tvb,
                                                       offset+(bit_offset/8),
                                                       0, chan+1);
            proto_item_append_text(no_tb_ti, " (of size %d)",
                                   p_fp_info->chan_tf_size[chan]);
            PROTO_ITEM_SET_GENERATED(no_tb_ti);
        }

        /* Show TBs from non-empty channels */
        for (n=0; n < p_fp_info->chan_num_tbs[chan]; n++)
        {
            proto_item *ti;
            if (data_tree)
            {
                ti = proto_tree_add_item(data_tree, hf_fp_tb, tvb,
                                         offset + (bit_offset/8),
                                         ((bit_offset % 8) + p_fp_info->chan_tf_size[chan] + 7) / 8,
                                         FALSE);
                proto_item_set_text(ti, "TB (chan %u, tb %u, %u bits)",
                                    chan+1, n+1, p_fp_info->chan_tf_size[chan]);
            }
            (*num_tbs)++;

            /* Advance bit offset */
            bit_offset += p_fp_info->chan_tf_size[chan];
            data_bits += p_fp_info->chan_tf_size[chan];

            /* Pad out to next byte */
            if (bit_offset % 8)
            {
                bit_offset += (8 - (bit_offset % 8));
            }
        }
    }

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "(%u bits in %u tbs)",
                        data_bits, *num_tbs);
    }

    /* Data tree should cover entire length */
    if (data_tree)
    {
        proto_item_set_len(tree_ti, bit_offset/8);
        proto_item_append_text(tree_ti, " (%u bits in %u tbs)", data_bits, *num_tbs);
    }

    /* Move offset past TBs (we know its already padded out to next byte) */
    offset += (bit_offset / 8);

    return offset;
}


/* Dissect the MAC-d PDUs of an HS-DSCH (type 1) frame.
   Length is in bits, and payload is offset by 4 bits of padding */
int dissect_macd_pdu_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                          int offset, guint16 length, guint16 number_of_pdus)
{
    int pdu;
    int bit_offset = 0;
    proto_item *pdus_ti = NULL;
    proto_tree *data_tree = NULL;

    /* Add data subtree */
    if (tree)
    {
        pdus_ti =  proto_tree_add_item(tree, hf_fp_data, tvb, offset, -1, FALSE);
        proto_item_set_text(pdus_ti, "%u MAC-d PDUs of %u bits", number_of_pdus, length);
        data_tree = proto_item_add_subtree(pdus_ti, ett_fp_data);
    }

    /* Now for the PDUs */
    for (pdu=0; pdu < number_of_pdus; pdu++)
    {
        proto_item *pdu_ti;

        if (data_tree)
        {
            /* Show 4 bits padding at start of PDU */
            proto_tree_add_item(data_tree, hf_fp_hsdsch_data_padding, tvb, offset+(bit_offset/8), 1, FALSE);
        }
        bit_offset += 4;

        /* Data bytes! */
        if (data_tree)
        {
            pdu_ti = proto_tree_add_item(data_tree, hf_fp_mac_d_pdu, tvb,
                                         offset + (bit_offset/8),
                                         ((bit_offset % 8) + length + 7) / 8,
                                         FALSE);
            proto_item_set_text(pdu_ti, "MAC-d PDU (PDU %u)", pdu+1);
        }

        /* Advance bit offset */
        bit_offset += length;

        /* Pad out to next byte */
        if (bit_offset % 8)
        {
            bit_offset += (8 - (bit_offset % 8));
        }
    }

    /* Data tree should cover entire length */
    proto_item_set_len(pdus_ti, bit_offset/8);

    /* Move offset past PDUs (we know its already padded out to next byte) */
    offset += (bit_offset / 8);

    /* Show summary in info column */
    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "   %u PDUs of %u bits",
                        number_of_pdus, length);
    }

    return offset;
}


/* Dissect the MAC-d PDUs of an HS-DSCH (type 2) frame.
   Length is in bytes, and payload is byte-aligned (no padding) */
int dissect_macd_pdu_data_type_2(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                 int offset, guint16 length, guint16 number_of_pdus)
{
    int pdu;
    proto_item *pdus_ti = NULL;
    proto_tree *data_tree = NULL;
    int first_offset = offset;

    /* Add data subtree */
    if (tree)
    {
        pdus_ti =  proto_tree_add_item(tree, hf_fp_data, tvb, offset, -1, FALSE);
        proto_item_set_text(pdus_ti, "%u MAC-d PDUs of %u bytes", number_of_pdus, length);
        data_tree = proto_item_add_subtree(pdus_ti, ett_fp_data);
    }

    /* Now for the PDUs */
    for (pdu=0; pdu < number_of_pdus; pdu++)
    {
        proto_item *pdu_ti;

        /* Data bytes! */
        if (data_tree)
        {
            pdu_ti = proto_tree_add_item(data_tree, hf_fp_mac_d_pdu, tvb,
                                         offset, length, FALSE);
            proto_item_set_text(pdu_ti, "MAC-d PDU (PDU %u)", pdu+1);
        }

        /* Advance offset */
        offset += length;
    }

    /* Data tree should cover entire length */
    proto_item_set_len(pdus_ti, offset-first_offset);

    /* Show summary in info column */
    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "   %u PDUs of %u bits",
                        number_of_pdus, length*8);
    }

    return offset;
}



/* Dissect CRCI bits (uplink) */
int dissect_crci_bits(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                      int num_tbs, int offset)
{
    int n;
    proto_item *ti = NULL;
    proto_tree *crcis_tree = NULL;
    guint errors = 0;

    /* Add CRCIs subtree */
    if (tree)
    {
        ti =  proto_tree_add_item(tree, hf_fp_crcis, tvb, offset, (num_tbs+7)/8, FALSE);
        proto_item_set_text(ti, "CRCI bits for %u tbs", num_tbs);
        crcis_tree = proto_item_add_subtree(ti, ett_fp_crcis);
    }

    /* CRCIs */
    for (n=0; n < num_tbs; n++)
    {
        int bit = (tvb_get_guint8(tvb, offset+(n/8)) >> (7-(n%8))) & 0x01;
        proto_tree_add_item(crcis_tree, hf_fp_crci[n%8], tvb, offset+(n/8),
                            1, FALSE);

        if (bit == 1)
        {
            errors++;
            expert_add_info_format(pinfo, ti,
                                   PI_CHECKSUM, PI_WARN,
                                   "CRCI error bit set for TB");
        }
    }

    if (tree)
    {
        /* Highlight range of bytes covered by indicator bits */
        proto_item_set_len(ti, (num_tbs+7) / 8);

        /* Show error count in root text */
        proto_item_append_text(ti, " (%u errors)", errors);
    }

    offset += ((num_tbs+7) / 8);
    return offset;
}


void dissect_spare_extension_and_crc(tvbuff_t *tvb, packet_info *pinfo,
                                     proto_tree *tree, guint8 dch_crc_present,
                                     int offset)
{
    int crc_size = 0;
    int remain = tvb_length_remaining(tvb, offset);
    proto_item *ti = NULL;

    /* Payload CRC (optional) */
    if (dch_crc_present == 1 || (dch_crc_present == 2 && remain >= 2))
    {
        crc_size = 2;
    }

    if (remain > crc_size)
    {
        ti = proto_tree_add_item(tree, hf_fp_spare_extension, tvb,
                                 offset, remain-crc_size, FALSE);
        proto_item_append_text(ti, " (%u octets)", remain-crc_size);
        expert_add_info_format(pinfo, ti,
                               PI_UNDECODED, PI_WARN,
                               "Spare Extension present (%u bytes)", remain-crc_size);
        offset += remain-crc_size;
    }

    if (crc_size)
    {
        proto_tree_add_item(tree, hf_fp_payload_crc, tvb, offset, crc_size,
                            FALSE);
    }
}



/***********************************************************/
/* Common control message types                            */

int dissect_common_outer_loop_power_control(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb,
                                            int offset, struct fp_info *p_fp_info _U_)
{
    return dissect_dch_outer_loop_power_control(tree, pinfo, tvb, offset);
}


int dissect_common_timing_adjustment(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb,
                                     int offset, struct fp_info *p_fp_info)
{
    if (p_fp_info->channel != CHANNEL_PCH)
    {
        guint8 cfn;
        gint16 toa;

        /* CFN control */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
        offset++;

        /* ToA */
        toa = tvb_get_ntohs(tvb, offset);
        proto_tree_add_item(tree, hf_fp_toa, tvb, offset, 2, FALSE);
        offset += 2;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "   CFN=%u, ToA=%d", cfn, toa);
        }
    }
    else
    {
        guint16 cfn;
        gint32 toa;

        /* PCH CFN is 12 bits */
        cfn = (tvb_get_ntohs(tvb, offset) >> 4);
        proto_tree_add_item(tree, hf_fp_pch_cfn, tvb, offset, 2, FALSE);
        offset += 2;

        /* 4 bits of padding follow... */

        /* 20 bits of ToA (followed by 4 padding bits) */
        toa = ((int)(tvb_get_ntoh24(tvb, offset) << 8)) / 4096;
        proto_tree_add_int(tree, hf_fp_pch_toa, tvb, offset, 3, toa);
        offset += 3;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "   CFN=%u, ToA=%d", cfn, toa);
        }
    }
    return offset;
}

int dissect_common_dl_node_synchronisation(packet_info *pinfo, proto_tree *tree,
                                           tvbuff_t *tvb, int offset)
{
    /* T1 (3 bytes) */
    guint32 t1 = tvb_get_ntoh24(tvb, offset);
    proto_tree_add_item(tree, hf_fp_t1, tvb, offset, 3, FALSE);
    offset += 3;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "   T1=%u", t1);
    }

    return offset;
}

int dissect_common_ul_node_synchronisation(packet_info *pinfo, proto_tree *tree,
                                           tvbuff_t *tvb, int offset)
{
    guint32 t1, t2, t3;

    /* T1 (3 bytes) */
    t1 = tvb_get_ntoh24(tvb, offset);
    proto_tree_add_item(tree, hf_fp_t1, tvb, offset, 3, FALSE);
    offset += 3;

    /* T2 (3 bytes) */
    t2 = tvb_get_ntoh24(tvb, offset);
    proto_tree_add_item(tree, hf_fp_t2, tvb, offset, 3, FALSE);
    offset += 3;

    /* T3 (3 bytes) */
    t3 = tvb_get_ntoh24(tvb, offset);
    proto_tree_add_item(tree, hf_fp_t3, tvb, offset, 3, FALSE);
    offset += 3;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "   T1=%u T2=%u, T3=%u",
                        t1, t2, t3);
    }

    return offset;
}

int dissect_common_dl_synchronisation(packet_info *pinfo, proto_tree *tree,
                                     tvbuff_t *tvb, int offset, struct fp_info *p_fp_info)
{
    guint16 cfn;

    if (p_fp_info->channel != CHANNEL_PCH)
    {
        /* CFN control */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
        offset++;
    }
    else
    {
        /* PCH CFN is 12 bits */
        cfn = (tvb_get_ntohs(tvb, offset) >> 4);
        proto_tree_add_item(tree, hf_fp_pch_cfn, tvb, offset, 2, FALSE);

        /* 4 bits of padding follow... */
        offset += 2;
    }

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "   CFN=%u", cfn);
    }

    return offset;
}

int dissect_common_ul_synchronisation(packet_info *pinfo, proto_tree *tree,
                                     tvbuff_t *tvb, int offset, struct fp_info *p_fp_info)
{
    return dissect_common_timing_adjustment(pinfo, tree, tvb, offset, p_fp_info);
}

int dissect_common_timing_advance(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb, int offset)
{
    guint8 cfn;
    guint16 timing_advance;

    /* CFN control */
    cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    /* Timing Advance */
    timing_advance = (tvb_get_guint8(tvb, offset) & 0x3f) * 4;
    proto_tree_add_uint(tree, hf_fp_timing_advance, tvb, offset, 1, timing_advance);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " CFN = %u, TA = %u",
                        cfn, timing_advance);
    }

    return offset;
}

int dissect_hsdpa_capacity_request(packet_info *pinfo, proto_tree *tree,
                                   tvbuff_t *tvb, int offset)
{
    guint8 priority;
    guint16 user_buffer_size;

    /* CmCH-PI */
    priority = (tvb_get_guint8(tvb, offset) & 0x0f);
    proto_tree_add_item(tree, hf_fp_cmch_pi, tvb, offset, 1, FALSE);
    offset++;

    /* User buffer size */
    user_buffer_size = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hf_fp_user_buffer_size, tvb, offset, 2, FALSE);
    offset += 2;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, "      CmCH-PI=%u  User-Buffer-Size=%u",
                        priority, user_buffer_size);
    }

    return offset;
}

int dissect_hsdpa_capacity_allocation(packet_info *pinfo, proto_tree *tree,
                                      tvbuff_t *tvb, int offset,
                                      struct fp_info *p_fp_info)
{
    proto_item *ti;
    proto_item *rate_ti;
    guint16 max_pdu_length;
    guint8  repetition_period;
    guint8  interval;
    guint64 credits;

    /* Congestion status (introduced sometime during R6...) */
    if ((p_fp_info->release == 6) || (p_fp_info->release == 7))
    {
        proto_tree_add_bits_item(tree, hf_fp_congestion_status, tvb,
                                 offset*8 + 2, 2, FALSE);
    }

    /* CmCH-PI */
    proto_tree_add_item(tree, hf_fp_cmch_pi, tvb, offset, 1, FALSE);
    offset++;

    /* Max MAC-d PDU length (13 bits) */
    max_pdu_length = tvb_get_ntohs(tvb, offset) >> 3;
    proto_tree_add_item(tree, hf_fp_hsdsch_max_macd_pdu_len, tvb, offset, 2, FALSE);
    offset++;

    /* HS-DSCH credits (11 bits) */
    ti = proto_tree_add_bits_ret_val(tree, hf_fp_hsdsch_credits, tvb,
                                     offset*8 + 5, 11, &credits, FALSE);
    offset += 2;

    /* Interesting values */
    if (credits == 0)
    {
        proto_item_append_text(ti, " (stop transmission)");
        expert_add_info_format(pinfo, ti,
                               PI_RESPONSE_CODE, PI_NOTE,
                               "Stop HSDPA transmission");
    }
    if (credits == 2047)
    {
        proto_item_append_text(ti, " (unlimited)");
    }

    /* HS-DSCH Interval */
    interval = tvb_get_guint8(tvb, offset);
    ti = proto_tree_add_uint(tree, hf_fp_hsdsch_interval, tvb, offset, 1, interval*10);
    offset++;
    if (interval == 0)
    {
        proto_item_append_text(ti, " (none of the credits shall be used)");
    }

    /* HS-DSCH Repetition period */
    repetition_period = tvb_get_guint8(tvb, offset);
    ti = proto_tree_add_item(tree, hf_fp_hsdsch_repetition_period, tvb, offset, 1, FALSE);
    offset++;
    if (repetition_period == 0)
    {
        proto_item_append_text(ti, " (unlimited repetition period)");
    }

    /* Calculated and show effective rate enabled */
    if (credits == 2047)
    {
        rate_ti = proto_tree_add_item(tree, hf_fp_hsdsch_unlimited_rate, tvb, 0, 0, FALSE);
        PROTO_ITEM_SET_GENERATED(rate_ti);
    }
    else
    {
        if (interval != 0)
        {
            /* Cast on credits is safe, since we know it won't exceed 10^11 */
            rate_ti = proto_tree_add_uint(tree, hf_fp_hsdsch_calculated_rate, tvb, 0, 0,
                                          (guint16)credits * max_pdu_length * (1000 / (interval*10)));
            PROTO_ITEM_SET_GENERATED(rate_ti);
        }
    }

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO,
                        "   Max-PDU-len=%u  Credits=%u  Interval=%u  Rep-Period=%u",
                        max_pdu_length, (guint16)credits, interval, repetition_period);
    }

    return offset;
}

int dissect_hsdpa_capacity_allocation_type_2(packet_info *pinfo, proto_tree *tree,
                                             tvbuff_t *tvb, int offset)
{
    proto_item *ti;
    proto_item *rate_ti;
    guint16    max_pdu_length;
    guint8     repetition_period;
    guint8     interval;
    guint16    credits;

    /* Congestion status */
    proto_tree_add_bits_item(tree, hf_fp_congestion_status, tvb,
                            offset*8 + 2, 2, FALSE);

    /* CmCH-PI */
    proto_tree_add_item(tree, hf_fp_cmch_pi, tvb, offset, 1, FALSE);
    offset++;

    /* 5 spare bits follow here */

    /* Max MAC-d/c PDU length (11 bits) */
    max_pdu_length = tvb_get_ntohs(tvb, offset) & 0x7ff;
    proto_tree_add_item(tree, hf_fp_hsdsch_max_macdc_pdu_len, tvb, offset, 2, FALSE);
    offset += 2;

    /* HS-DSCH credits (16 bits) */
    credits = (tvb_get_ntohs(tvb, offset));
    ti = proto_tree_add_uint(tree, hf_fp_hsdsch_credits, tvb,
                             offset, 2, credits);
    offset += 2;

    /* Interesting values */
    if (credits == 0)
    {
        proto_item_append_text(ti, " (stop transmission)");
        expert_add_info_format(pinfo, ti,
                               PI_RESPONSE_CODE, PI_NOTE,
                               "Stop HSDPA transmission");
    }
    if (credits == 65535)
    {
        proto_item_append_text(ti, " (unlimited)");
    }

    /* HS-DSCH Interval */
    interval = tvb_get_guint8(tvb, offset);
    ti = proto_tree_add_uint(tree, hf_fp_hsdsch_interval, tvb, offset, 1, interval*10);
    offset++;
    if (interval == 0)
    {
        proto_item_append_text(ti, " (none of the credits shall be used)");
    }

    /* HS-DSCH Repetition period */
    repetition_period = tvb_get_guint8(tvb, offset);
    ti = proto_tree_add_item(tree, hf_fp_hsdsch_repetition_period, tvb, offset, 1, FALSE);
    offset++;
    if (repetition_period == 0)
    {
        proto_item_append_text(ti, " (unlimited repetition period)");
    }

    /* Calculated and show effective rate enabled */
    if (credits == 65535)
    {
        rate_ti = proto_tree_add_item(tree, hf_fp_hsdsch_unlimited_rate, tvb, 0, 0, FALSE);
        PROTO_ITEM_SET_GENERATED(rate_ti);
    }
    else
    {
        if (interval != 0)
        {
            rate_ti = proto_tree_add_uint(tree, hf_fp_hsdsch_calculated_rate, tvb, 0, 0,
                                          credits * max_pdu_length * (1000 / (interval*10)));
            PROTO_ITEM_SET_GENERATED(rate_ti);
        }
    }

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO,
                        "   Max-PDU-len=%u  Credits=%u  Interval=%u  Rep-Period=%u",
                        max_pdu_length, credits, interval, repetition_period);
    }

    return offset;
}



int dissect_common_dynamic_pusch_assignment(packet_info *pinfo, proto_tree *tree,
                                            tvbuff_t *tvb, int offset)
{
    guint8 pusch_set_id;
    guint8 activation_cfn;
    guint8 duration;

    /* PUSCH Set Id */
    pusch_set_id = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_pusch_set_id, tvb, offset, 1, FALSE);
    offset++;

    /* Activation CFN */
    activation_cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_activation_cfn, tvb, offset, 1, FALSE);
    offset++;

    /* Duration */
    duration = tvb_get_guint8(tvb, offset) * 10;
    proto_tree_add_uint(tree, hf_fp_duration, tvb, offset, 1, duration);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO,
                        "   PUSCH Set Id=%u  Activation CFN=%u  Duration=%u",
                        pusch_set_id, activation_cfn, duration);
    }

    return offset;
}





/* Dissect the control part of a common channel message */
void dissect_common_control(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                            int offset, struct fp_info *p_fp_info)
{
    /* Common control frame type */
    guint8 control_frame_type = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_common_control_frame_type, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO,
                       val_to_str(control_frame_type, common_control_frame_type_vals, "Unknown"));
    }

    /* Frame-type specific dissection */
    switch (control_frame_type)
    {
        case COMMON_OUTER_LOOP_POWER_CONTROL:
            offset = dissect_common_outer_loop_power_control(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case COMMON_TIMING_ADJUSTMENT:
            offset = dissect_common_timing_adjustment(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case COMMON_DL_SYNCHRONISATION:
            offset = dissect_common_dl_synchronisation(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case COMMON_UL_SYNCHRONISATION:
            offset = dissect_common_ul_synchronisation(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case COMMON_DL_NODE_SYNCHRONISATION:
            offset = dissect_common_dl_node_synchronisation(pinfo, tree, tvb, offset);
            break;
        case COMMON_UL_NODE_SYNCHRONISATION:
            offset = dissect_common_ul_node_synchronisation(pinfo, tree, tvb, offset);
            break;
        case COMMON_DYNAMIC_PUSCH_ASSIGNMENT:
            offset = dissect_common_dynamic_pusch_assignment(pinfo, tree, tvb, offset);
            break;
        case COMMON_TIMING_ADVANCE:
            offset = dissect_common_timing_advance(pinfo, tree, tvb, offset);
            break;
        case COMMON_HS_DSCH_Capacity_Request:
            offset = dissect_hsdpa_capacity_request(pinfo, tree, tvb, offset);
            break;
        case COMMON_HS_DSCH_Capacity_Allocation:
            offset = dissect_hsdpa_capacity_allocation(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case COMMON_HS_DSCH_Capacity_Allocation_Type_2:
            offset = dissect_hsdpa_capacity_allocation_type_2(pinfo, tree, tvb, offset);
            break;

        default:
            break;
    }

    /* Spare Extension */
    dissect_spare_extension_and_crc(tvb, pinfo, tree, 0, offset);
}



/**************************/
/* Dissect a RACH channel */
void dissect_rach_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                               int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;
        guint8 cfn;
        guint32 propagation_delay = 0;
        proto_item *propagation_delay_ti = NULL;
        guint32 received_sync_ul_timing_deviation = 0;
        proto_item *received_sync_ul_timing_deviation_ti = NULL;
        proto_item *rx_timing_deviation_ti = NULL;
        guint16     rx_timing_deviation = 0;

        /* DATA */

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* TFI */
        proto_tree_add_item(tree, hf_fp_tfi, tvb, offset, 1, FALSE);
        offset++;

        if (p_fp_info->channel == CHANNEL_RACH_FDD)
        {
            /* Propagation delay */
            propagation_delay = tvb_get_guint8(tvb, offset);
            propagation_delay_ti = proto_tree_add_uint(tree, hf_fp_propagation_delay, tvb, offset, 1,
                                                       propagation_delay*3);
            offset++;
        }

        /* Should be TDD 3.84 or 7.68 */
        if (p_fp_info->channel == CHANNEL_RACH_TDD)
        {
            /* Rx Timing Deviation */
            rx_timing_deviation = tvb_get_guint8(tvb, offset);
            rx_timing_deviation_ti = proto_tree_add_item(tree, hf_fp_rx_timing_deviation, tvb, offset, 1, FALSE);
            offset++;
        }

        if (p_fp_info->channel == CHANNEL_RACH_TDD_128)
        {
            /* Received SYNC UL Timing Deviation */
            received_sync_ul_timing_deviation = tvb_get_guint8(tvb, offset);
            received_sync_ul_timing_deviation_ti =
                 proto_tree_add_item(tree, hf_fp_received_sync_ul_timing_deviation, tvb, offset, 1, FALSE);
            offset++;
        }

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* CRCIs */
        offset = dissect_crci_bits(tvb, pinfo, tree, num_tbs, offset);

        /* Info introduced in R6 */
        if ((p_fp_info->release == 6) ||
            (p_fp_info->release == 7))
        {
            int n;
            guint8 flags;
            guint8 flag_bytes = 0;

            gboolean cell_portion_id_present = FALSE;
            gboolean ext_propagation_delay_present = FALSE;
            gboolean angle_of_arrival_present = FALSE;
            gboolean ext_rx_sync_ul_timing_deviation_present = FALSE;
            gboolean ext_rx_timing_deviation_present = FALSE;

            /* New IE flags (assume mandatory for now) */
            do
            {
                proto_item *new_ie_flags_ti;
                proto_tree *new_ie_flags_tree;
                guint ies_found = 0;

                /* Add new IE flags subtree */
                new_ie_flags_ti = proto_tree_add_string_format(tree, hf_fp_rach_new_ie_flags, tvb, offset, 1,
                                                              "", "New IE flags");
                new_ie_flags_tree = proto_item_add_subtree(new_ie_flags_ti, ett_fp_rach_new_ie_flags);

                /* Read next byte */
                flags = tvb_get_guint8(tvb, offset);
                flag_bytes++;

                /* Dissect individual bits */
                for (n=0; n < 8; n++)
                {
                    switch (n) {
                        case 6:
                            switch (p_fp_info->division)
                            {
                                case Division_FDD:
                                    /* Ext propagation delay */
                                    ext_propagation_delay_present = TRUE;
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_ext_propagation_delay_present,
                                                        tvb, offset, 1, FALSE);
                                    break;
                                case Division_TDD_128:
                                    /* Ext Rx Sync UL Timing */
                                    ext_rx_sync_ul_timing_deviation_present = TRUE;
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_ext_rx_sync_ul_timing_deviation_present,
                                                        tvb, offset, 1, FALSE);
        
                                    break;
                                default:
                                    /* Not defined */
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_new_ie_flag_unused[n],
                                                        tvb, offset, 1, FALSE);
                                    break;
                            }
                            break;
                        case 7:
                            switch (p_fp_info->division)
                            {
                                case Division_FDD:
                                    /* Cell Portion ID */
                                    cell_portion_id_present = TRUE;
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_cell_portion_id_present,
                                                        tvb, offset, 1, FALSE);
                                    break;
                                case Division_TDD_128:
                                    /* AOA */
                                    angle_of_arrival_present = TRUE;
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_angle_of_arrival_present,
                                                        tvb, offset, 1, FALSE);
                                    break;
                                case Division_TDD_384:
                                case Division_TDD_768:
                                    /* Extended Rx Timing Deviation */
                                    ext_rx_timing_deviation_present = TRUE;
                                    proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_ext_rx_timing_deviation_present,
                                                        tvb, offset, 1, FALSE);
                                    break;
                            }
                            break;

                        default:
                            /* No defined meanings */
                            proto_tree_add_item(new_ie_flags_tree, hf_fp_rach_new_ie_flag_unused[n],
                                                tvb, offset, 1, FALSE);
                            break;
                    }
                    if ((flags >> (7-n)) & 0x01)
                    {
                        ies_found++;
                    }
                }
                offset++;

                proto_item_append_text(new_ie_flags_ti, " (%u IEs found)", ies_found);

                /* Last bit set will indicate another flags byte follows... */
            } while (0); /*((flags & 0x01) && (flag_bytes < 31));*/

            /* Cell Portion ID */
            if (cell_portion_id_present) {
                    proto_tree_add_item(tree, hf_fp_cell_portion_id, tvb, offset, 1, FALSE);
                    offset++;
            }

            /* Ext Rx Timing Deviation */
            if (ext_rx_timing_deviation_present)
            {
                guint8 extra_bits;
                guint bits_to_extend;
                switch (p_fp_info->division)
                {
                    case Division_TDD_384:
                        bits_to_extend = 1;
                        break;
                    case Division_TDD_768:
                        bits_to_extend = 2;
                        break;
    
                    default:
                        /* TODO: report unexpected division type */
                        bits_to_extend = 1;
                        break;
                }
                extra_bits = tvb_get_guint8(tvb, offset) &
                                 ((bits_to_extend == 1) ? 0x01 : 0x03);
                rx_timing_deviation = (extra_bits << 8) | (rx_timing_deviation);
                proto_item_append_text(rx_timing_deviation_ti,
                                       " (extended to 0x%x)",
                                       rx_timing_deviation);
                proto_tree_add_bits_item(tree, hf_fp_extended_bits, tvb,
                                         offset*8 + (8-bits_to_extend), bits_to_extend, FALSE);
                offset++;
            }

            /* Ext propagation delay. */
            if (ext_propagation_delay_present)
            {
                guint16 extra_bits = tvb_get_ntohs(tvb, offset) & 0x03ff;
                proto_tree_add_item(tree, hf_fp_ext_propagation_delay, tvb, offset, 2, FALSE);

                /* Adding 10 bits to original 8 */
                proto_item_append_text(propagation_delay_ti, " (extended to %u)",
                                       ((extra_bits << 8) | propagation_delay) * 3);
                offset += 2;
            }

            /* Angle of Arrival (AOA) */
            if (angle_of_arrival_present)
            {
                proto_tree_add_item(tree, hf_fp_angle_of_arrival, tvb, offset, 2, FALSE);
                offset += 2;
            }

            /* Ext. Rx Sync UL Timing Deviation */
            if (ext_rx_sync_ul_timing_deviation_present) {
                guint16 extra_bits;

                /* Ext received Sync UL Timing Deviation */
                extra_bits = tvb_get_ntohs(tvb, offset) & 0x1fff;
                proto_tree_add_item(tree, hf_fp_ext_received_sync_ul_timing_deviation, tvb, offset, 2, FALSE);

                /* Adding 13 bits to original 8 */
                proto_item_append_text(received_sync_ul_timing_deviation_ti, " (extended to %u)",
                                       (extra_bits << 8) | received_sync_ul_timing_deviation);
                offset += 2;
            }
        }

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/**************************/
/* Dissect a FACH channel */
void dissect_fach_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                               int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;
        guint8 cfn;

        /* DATA */

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* TFI */
        proto_tree_add_item(tree, hf_fp_fach_tfi, tvb, offset, 1, FALSE);
        offset++;

        /* Transmit power level */
        proto_tree_add_float(tree, hf_fp_transmit_power_level, tvb, offset, 1,
                             (float)(int)(tvb_get_guint8(tvb, offset)) / 10);
        offset++;

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* New IE flags (if it looks as though they are present) */
        if ((p_fp_info->release == 7) &&
            (tvb_length_remaining(tvb, offset) > 2))
        {
            guint8 flags = tvb_get_guint8(tvb, offset);
            guint8 aoa_present = flags & 0x01;
            offset++;

            if (aoa_present)
            {
                proto_tree_add_item(tree, hf_fp_angle_of_arrival, tvb, offset, 2, FALSE);
                offset += 2;
            }
        }

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/**************************/
/* Dissect a DSCH channel */
void dissect_dsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                               int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;
        guint8 cfn;

        /* DATA */

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* TFI */
        proto_tree_add_item(tree, hf_fp_tfi, tvb, offset, 1, FALSE);
        offset++;


        /* Other fields depend upon release & FDD/TDD settings */
        if (((p_fp_info->release == 99) || (p_fp_info->release == 4)) &&
             (p_fp_info->channel == CHANNEL_DSCH_FDD))
        {
            /* Power offset */
            proto_tree_add_float(tree, hf_fp_power_offset, tvb, offset, 1,
                                 (float)(-32.0) +
                                  ((float)(int)(tvb_get_guint8(tvb, offset)) * (float)(0.25)));
            offset++;

            /* Code number */
            proto_tree_add_item(tree, hf_fp_code_number, tvb, offset, 1, FALSE);
            offset++;

            /* Spreading Factor (3 bits) */
            proto_tree_add_item(tree, hf_fp_spreading_factor, tvb, offset, 1, FALSE);

            /* MC info (4 bits)*/
            proto_tree_add_item(tree, hf_fp_mc_info, tvb, offset, 1, FALSE);

            /* Last bit of this byte is spare */
            offset++;
        }
        else
        {
            /* Normal case */

            /* PDSCH Set Id */
            proto_tree_add_item(tree, hf_fp_pdsch_set_id, tvb, offset, 1, FALSE);
            offset++;

            /* Transmit power level */
            proto_tree_add_float(tree, hf_fp_transmit_power_level, tvb, offset, 1,
                                 (float)(int)(tvb_get_guint8(tvb, offset)) / 10);
            offset++;
        }

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/**************************/
/* Dissect a USCH channel */
void dissect_usch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                               int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;
        guint cfn;
        guint16 rx_timing_deviation;
        proto_item *rx_timing_deviation_ti;

        /* DATA */

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* TFI */
        proto_tree_add_item(tree, hf_fp_usch_tfi, tvb, offset, 1, FALSE);
        offset++;

        /* Rx Timing Deviation */
        rx_timing_deviation = tvb_get_guint8(tvb, offset);
        rx_timing_deviation_ti = proto_tree_add_item(tree, hf_fp_rx_timing_deviation,
                                                     tvb, offset, 1, FALSE);
        offset++;

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* QE */
        proto_tree_add_item(tree, hf_fp_quality_estimate, tvb, offset, 1, FALSE);
        offset++;

        /* CRCIs */
        offset = dissect_crci_bits(tvb, pinfo, tree, num_tbs, offset);

        /* New IEs */
        if ((p_fp_info->release == 7) &&
            (tvb_length_remaining(tvb, offset) > 2))
        {
            guint8 flags = tvb_get_guint8(tvb, offset);
            guint8 bits_extended = flags & 0x01;
            offset++;

            if (bits_extended)
            {
                guint8 extra_bits = tvb_get_guint8(tvb, offset) & 0x03;
                proto_item_append_text(rx_timing_deviation_ti,
                                       " (extended to %u)",
                                       (rx_timing_deviation << 2) | extra_bits);
            }
            offset++;
        }

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}



/**************************/
/* Dissect a PCH channel  */
void dissect_pch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                              int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;
    guint16  pch_cfn;
    gboolean paging_indication;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;

        /* DATA */

        /* 12-bit CFN value */
        proto_tree_add_item(tree, hf_fp_pch_cfn, tvb, offset, 2, FALSE);
        pch_cfn = (tvb_get_ntohs(tvb, offset) & 0xfff0) >> 4;
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%04u ", pch_cfn);
        }

        /* Paging indication */
        proto_tree_add_item(tree, hf_fp_pch_pi, tvb, offset, 1, FALSE);
        paging_indication = tvb_get_guint8(tvb, offset) & 0x01;
        offset++;

        /* 5-bit TFI */
        proto_tree_add_item(tree, hf_fp_pch_tfi, tvb, offset, 1, FALSE);
        offset++;

        /* Optional paging indications */
        if (paging_indication)
        {
            proto_item *ti;
            ti = proto_tree_add_item(tree, hf_fp_paging_indication_bitmap, tvb,
                                     offset,
                                     (p_fp_info->paging_indications+7) / 8,
                                     FALSE);
            proto_item_append_text(ti, " (%u bits)", p_fp_info->paging_indications);
            offset += ((p_fp_info->paging_indications+7) / 8);
        }

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/**************************/
/* Dissect a CPCH channel */
void dissect_cpch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                               int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        int num_tbs = 0;
        guint cfn;

        /* DATA */

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* TFI */
        proto_tree_add_item(tree, hf_fp_cpch_tfi, tvb, offset, 1, FALSE);
        offset++;

        /* Propagation delay */
        proto_tree_add_uint(tree, hf_fp_propagation_delay, tvb, offset, 1,
                            tvb_get_guint8(tvb, offset) * 3);
        offset++;

        /* TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* CRCIs */
        offset = dissect_crci_bits(tvb, pinfo, tree, num_tbs, offset);

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/**************************/
/* Dissect a BCH channel  */
void dissect_bch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                              int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
}


/********************************/
/* Dissect an IUR DSCH channel  */
void dissect_iur_dsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                   int offset, struct fp_info *p_fp_info _U_)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        /* TODO: DATA */
    }
}




/************************/
/* DCH control messages */

int dissect_dch_timing_adjustment(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    guint8 control_cfn;
    gint16 toa;

    /* CFN control */
    control_cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    /* ToA */
    toa = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hf_fp_toa, tvb, offset, 2, FALSE);
    offset += 2;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO,
                        " CFN = %u, ToA = %d", control_cfn, toa);
    }

    return offset;
}

int dissect_dch_rx_timing_deviation(packet_info *pinfo, proto_tree *tree,
                                    tvbuff_t *tvb, int offset,
                                    struct fp_info *p_fp_info)
{
    guint16 timing_deviation = 0;
    gint timing_deviation_chips = 0;
    proto_item *timing_deviation_ti = NULL;

    /* CFN control */
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    /* Rx Timing Deviation */
    timing_deviation = tvb_get_guint8(tvb, offset);
    timing_deviation_ti = proto_tree_add_item(tree, hf_fp_dch_rx_timing_deviation, tvb, offset, 1, FALSE);
    offset++;

    /* May be extended in R7, but in this case there are at least 2 bytes remaining */
    if ((p_fp_info->release == 7) &&
        (tvb_length_remaining(tvb, offset) >= 2))
    {
        /* New IE flags */
        guint64 extended_bits_present;
        guint64 e_rucch_present;

        /* Read flags */
        proto_tree_add_bits_ret_val(tree, hf_fp_e_rucch_present, tvb,
                                    offset*8 + 6, 1, &e_rucch_present, FALSE);
        proto_tree_add_bits_ret_val(tree, hf_fp_extended_bits_present, tvb,
                                    offset*8 + 7, 1, &extended_bits_present, FALSE);
        offset++;

        /* Optional E-RUCCH */
        if (e_rucch_present)
        {
            /* Value of bit_offset depends upon division type */
            int bit_offset;

            switch (p_fp_info->division)
            {
                case Division_TDD_384:
                    bit_offset = 6;
                    break;
                case Division_TDD_768:
                    bit_offset = 5;
                    break;
                default:
                    {
                        proto_item *ti = proto_tree_add_text(tree, tvb, 0, 0,
                                                             "Error: expecting TDD-384 or TDD-768");
                        PROTO_ITEM_SET_GENERATED(ti);
                        expert_add_info_format(pinfo, ti,
                                               PI_MALFORMED, PI_NOTE,
                                               "Error: expecting TDD-384 or TDD-768");
                        bit_offset = 6;
                    }
                    
            }

            proto_tree_add_item(tree, hf_fp_dch_e_rucch_flag, tvb, offset, 1, FALSE);
            proto_tree_add_bits_item(tree, hf_fp_dch_e_rucch_flag, tvb,
                                     offset*8 + bit_offset, 1, FALSE);
        }

        /* Timing deviation may be extended by another:
           - 1 bits (3.84 TDD)    OR
           - 2 bits (7.68 TDD)
        */
        if (extended_bits_present)
        {
            guint8 extra_bits;
            guint bits_to_extend;
            switch (p_fp_info->division)
            {
                case Division_TDD_384:
                    bits_to_extend = 1;
                    break;
                case Division_TDD_768:
                    bits_to_extend = 2;
                    break;

                default:
                    /* TODO: report unexpected division type */
                    bits_to_extend = 1;
                    break;
            }
            extra_bits = tvb_get_guint8(tvb, offset) &
                             ((bits_to_extend == 1) ? 0x01 : 0x03);
            timing_deviation = (extra_bits << 8) | (timing_deviation);
            proto_item_append_text(timing_deviation_ti,
                                   " (extended to 0x%x)",
                                   timing_deviation);
            proto_tree_add_bits_item(tree, hf_fp_extended_bits, tvb,
                                     offset*8 + (8-bits_to_extend), bits_to_extend, FALSE);
            offset++;
        }
    }

    timing_deviation_chips = (timing_deviation*4) - 1024;
    proto_item_append_text(timing_deviation_ti, " (%d chips)",
                           timing_deviation_chips);

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " deviation = %u (%d chips)",
                        timing_deviation, timing_deviation_chips);
    }

    return offset;
}

int dissect_dch_dl_synchronisation(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    /* CFN control */
    guint cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " CFN = %u", cfn);
    }

    return offset;
}

int dissect_dch_ul_synchronisation(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    guint8 cfn;
    gint16 toa;

    /* CFN control */
    cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    /* ToA */
    toa = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hf_fp_toa, tvb, offset, 2, FALSE);
    offset += 2;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " CFN = %u, ToA = %d",
                        cfn, toa);
    }

    return offset;
}

int dissect_dch_outer_loop_power_control(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    /* UL SIR target */
    float target = (float)-8.2 + ((float)0.1 * (float)(int)(tvb_get_guint8(tvb, offset)));
    proto_tree_add_float(tree, hf_fp_ul_sir_target, tvb, offset, 1, target);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " UL SIR Target = %f", target);
    }

    return offset;
}

int dissect_dch_dl_node_synchronisation(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    return dissect_common_dl_node_synchronisation(pinfo, tree, tvb, offset);
}

int dissect_dch_ul_node_synchronisation(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    return dissect_common_ul_node_synchronisation(pinfo, tree, tvb, offset);
}

int dissect_dch_radio_interface_parameter_update(proto_tree *tree, packet_info *pinfo _U_, tvbuff_t *tvb, int offset)
{
    int n;
    guint8 cfn;
    guint8 value;

    /* Show defined flags in these 2 bytes */
    for (n=4; n >= 0; n--)
    {
        proto_tree_add_item(tree, hf_fp_radio_interface_parameter_update_flag[n], tvb, offset, 2, FALSE);
    }
    offset += 2;

    /* CFN  */
    cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
    offset++;

    /* DPC mode */
    proto_tree_add_item(tree, hf_fp_dpc_mode, tvb, offset, 1, FALSE);

    /* TPC PO */
    proto_tree_add_item(tree, hf_fp_tpc_po, tvb, offset, 1, FALSE);
    offset++;

    /* Multiple RL sets indicator */
    proto_tree_add_item(tree, hf_fp_multiple_rl_set_indicator, tvb, offset, 1, FALSE);
    offset += 2;

    /* MAX_UE_TX_POW */
    value = (tvb_get_guint8(tvb, offset) & 0x7f);
    proto_tree_add_int(tree, hf_fp_max_ue_tx_pow, tvb, offset, 1, -55 + value);
    offset++;

    return offset;
}

int dissect_dch_timing_advance(proto_tree *tree, packet_info *pinfo,
                               tvbuff_t *tvb, int offset, struct fp_info *p_fp_info)
{
    guint8 cfn;
    guint16 timing_advance;
    proto_item *timing_advance_ti;

    /* CFN control */
    cfn = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_cfn_control, tvb, offset, 1, FALSE);
    offset++;

    /* Timing Advance */
    timing_advance = (tvb_get_guint8(tvb, offset) & 0x3f) * 4;
    timing_advance_ti = proto_tree_add_uint(tree, hf_fp_timing_advance, tvb, offset, 1, timing_advance);
    offset++;

    if ((p_fp_info->release == 7) &&
        (tvb_length_remaining(tvb, offset) > 0))
    {
        /* New IE flags */
        guint8 flags = tvb_get_guint8(tvb, offset);
        guint8 extended_bits = flags & 0x01;
        offset++;

        if (extended_bits)
        {
            guint8 extra_bit = tvb_get_guint8(tvb, offset) & 0x01;
            proto_item_append_text(timing_advance_ti, " (extended to %u)",
                                   (timing_advance << 1) | extra_bit);
        }
        offset++;
    }


    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " CFN = %u, TA = %u",
                        cfn, timing_advance);
    }

    return offset;
}

int dissect_dch_tnl_congestion_indication(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset)
{
    guint64 status;

    /* Congestion status */
    proto_tree_add_bits_ret_val(tree, hf_fp_congestion_status, tvb,
                                offset*8 + 6, 2, &status, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_fstr(pinfo->cinfo, COL_INFO, " status = %s",
                        val_to_str((guint16)status, congestion_status_vals, "unknown"));
    }

    return offset;
}




/* DCH control frame */
void dissect_dch_control_frame(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int offset,
                               struct fp_info *p_fp_info)
{
    /* Control frame type */
    guint8 control_frame_type = tvb_get_guint8(tvb, offset);
    proto_tree_add_item(tree, hf_fp_dch_control_frame_type, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO,
                       val_to_str(control_frame_type,
                                  dch_control_frame_type_vals, "Unknown"));
    }

    switch (control_frame_type)
    {
        case DCH_TIMING_ADJUSTMENT:
            offset = dissect_dch_timing_adjustment(tree, pinfo, tvb, offset);
            break;
        case DCH_RX_TIMING_DEVIATION:
            offset = dissect_dch_rx_timing_deviation(pinfo, tree, tvb, offset, p_fp_info);
            break;
        case DCH_DL_SYNCHRONISATION:
            offset = dissect_dch_dl_synchronisation(tree, pinfo, tvb, offset);
            break;
        case DCH_UL_SYNCHRONISATION:
            offset = dissect_dch_ul_synchronisation(tree, pinfo, tvb, offset);
            break;
        case DCH_OUTER_LOOP_POWER_CONTROL:
            offset = dissect_dch_outer_loop_power_control(tree, pinfo, tvb, offset);
            break;
        case DCH_DL_NODE_SYNCHRONISATION:
            offset = dissect_dch_dl_node_synchronisation(tree, pinfo, tvb, offset);
            break;
        case DCH_UL_NODE_SYNCHRONISATION:
            offset = dissect_dch_ul_node_synchronisation(tree, pinfo, tvb, offset);
            break;
        case DCH_RADIO_INTERFACE_PARAMETER_UPDATE:
            offset = dissect_dch_radio_interface_parameter_update(tree, pinfo, tvb, offset);
            break;
        case DCH_TIMING_ADVANCE:
            offset = dissect_dch_timing_advance(tree, pinfo, tvb, offset, p_fp_info);
            break;
        case DCH_TNL_CONGESTION_INDICATION:
            offset = dissect_dch_tnl_congestion_indication(tree, pinfo, tvb, offset);
            break;
    }

    /* Spare Extension */
    dissect_spare_extension_and_crc(tvb, pinfo, tree, 0, offset);
}

/*******************************/
/* Dissect a DCH channel       */
void dissect_dch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                              int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;
    guint8   cfn;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO,
                       is_control_frame ? " [Control] " :
                                          ((p_fp_info->is_uplink) ? " [ULData] " :
                                                                    " [DLData] " ));
    }

    if (is_control_frame)
    {
        /* DCH control frame */
        dissect_dch_control_frame(tree, pinfo, tvb, offset, p_fp_info);
    }
    else
    {
        /************************/
        /* DCH data here        */
        int chan;
        int num_tbs = 0;

        /* CFN */
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        cfn = tvb_get_guint8(tvb, offset);
        offset++;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "CFN=%03u ", cfn);
        }

        /* One TFI for each channel */
        for (chan=0; chan < p_fp_info->num_chans; chan++)
        {
            proto_tree_add_item(tree, hf_fp_tfi, tvb, offset, 1, FALSE);
            offset++;
        }

        /* Dissect TB data */
        offset = dissect_tb_data(tvb, pinfo, tree, offset, p_fp_info, &num_tbs);

        /* QE (uplink only) */
        if (p_fp_info->is_uplink)
        {
            proto_tree_add_item(tree, hf_fp_quality_estimate, tvb, offset, 1, FALSE);
            offset++;
        }

        /* CRCI bits (uplink only) */
        if (p_fp_info->is_uplink)
        {
            offset = dissect_crci_bits(tvb, pinfo, tree, num_tbs, offset);
        }

        /* Spare extension and payload CRC (optional) */
        dissect_spare_extension_and_crc(tvb, pinfo, tree,
                                        p_fp_info->dch_crc_present, offset);
    }
}



/**********************************/
/* Dissect an E-DCH channel       */
void dissect_e_dch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;
    guint8   number_of_subframes;
    guint8   cfn;
    int      n;
    struct   subframe_info subframes[16];

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_edch_header_crc, tvb, offset, 2, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        /* DCH control frame */
        dissect_dch_control_frame(tree, pinfo, tvb, offset, p_fp_info);
    }
    else
    {
        /********************************/
        /* E-DCH data here              */

        guint  bit_offset = 0;
        guint  total_pdus = 0;
        guint  total_bits = 0;

        /* FSN */
        proto_tree_add_item(tree, hf_fp_edch_fsn, tvb, offset, 1, FALSE);
        offset++;

        /* Number of subframes.
           This was 3 bits in early releases, is 4 bits offset by 1 in later releases  */
        if ((p_fp_info->release >= 6) &&
            ((p_fp_info->release_year > 2005) ||
             (p_fp_info->release_year == 2005 && p_fp_info->release_month >= 9)))
        {
            /* Use 4 bits plus offset of 1 */
            number_of_subframes = (tvb_get_guint8(tvb, offset) & 0x0f) + 1;
        }
        else
        {
            /* Use 3 bits only */
            number_of_subframes = (tvb_get_guint8(tvb, offset) & 0x07);
        }
        proto_tree_add_uint(tree, hf_fp_edch_number_of_subframes, tvb, offset, 1,
                            number_of_subframes);

        offset++;

        /* CFN */
        cfn = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_cfn, tvb, offset, 1, FALSE);
        offset++;

        /* EDCH subframe header list */
        for (n=0; n < number_of_subframes; n++)
        {
            int i;
            int start_offset = offset;
            proto_item *subframe_header_ti;
            proto_tree *subframe_header_tree;

            /* Add subframe header subtree */
            subframe_header_ti = proto_tree_add_string_format(tree, hf_fp_edch_subframe_header, tvb, offset, 0,
                                                              "", "Subframe");
            subframe_header_tree = proto_item_add_subtree(subframe_header_ti, ett_fp_edch_subframe_header);

            /* Number of HARQ Retransmissions */
            proto_tree_add_item(subframe_header_tree, hf_fp_edch_harq_retransmissions, tvb,
                                offset, 1, FALSE);

            /* Subframe number */
            subframes[n].subframe_number = (tvb_get_guint8(tvb, offset) & 0x07);
            proto_tree_add_item(subframe_header_tree, hf_fp_edch_subframe_number, tvb,
                                offset, 1, FALSE);
            offset++;

            /* Number of MAC-es PDUs */
            subframes[n].number_of_mac_es_pdus = (tvb_get_guint8(tvb, offset) & 0xf0) >> 4;
            proto_tree_add_item(subframe_header_tree, hf_fp_edch_number_of_mac_es_pdus,
                                tvb, offset, 1, FALSE);
            bit_offset = 4;

            proto_item_append_text(subframe_header_ti, " %u header (%u MAC-es PDUs)",
                                   subframes[n].subframe_number,
                                   subframes[n].number_of_mac_es_pdus);

            /* Details of each MAC-es PDU */
            for (i=0; i < subframes[n].number_of_mac_es_pdus; i++)
            {
                guint64 ddi;
                int     ddi_offset;
                guint64 n_pdus;
                int     n_pdus_offset;

                /* DDI (6 bits) */
                ddi_offset = offset + (bit_offset / 8);

                proto_tree_add_bits_ret_val(subframe_header_tree, hf_fp_edch_ddi, tvb,
                                            offset*8 + bit_offset, 6, &ddi, FALSE);

                subframes[n].ddi[i] = (guint8)ddi;
                bit_offset += 6;

                /* Number of MAC-d PDUs (6 bits) */
                n_pdus_offset = offset + (bit_offset / 8);

                proto_tree_add_bits_ret_val(subframe_header_tree, hf_fp_edch_number_of_mac_d_pdus, tvb,
                                            offset*8 + bit_offset, 6, &n_pdus, FALSE);

                subframes[n].number_of_mac_d_pdus[i] = (guint8)n_pdus;
                bit_offset += 6;
            }

            offset += ((bit_offset+7)/8);

            /* Tree should cover entire subframe header */
            proto_item_set_len(subframe_header_ti, offset - start_offset);
        }

        /* EDCH subframes */
        bit_offset = 0;
        for (n=0; n < number_of_subframes; n++)
        {
            int i;
            proto_item *subframe_ti;
            proto_tree *subframe_tree;
            guint bits_in_subframe = 0;
            guint mac_d_pdus_in_subframe = 0;

            bit_offset = 0;

            /* Add subframe subtree */
            subframe_ti = proto_tree_add_string_format(tree, hf_fp_edch_subframe, tvb, offset, 0,
                                                       "", "Subframe %u data", subframes[n].subframe_number);
            subframe_tree = proto_item_add_subtree(subframe_ti, ett_fp_edch_subframe);

            for (i=0; i < subframes[n].number_of_mac_es_pdus; i++)
            {
                int         m;
                guint16     size = 0;
                guint8      tsn;
                guint       send_size;
                proto_item  *ti;

                /* Look up mac-d pdu size for this ddi */
                for (m=0; m < p_fp_info->no_ddi_entries; m++)
                {
                    if (subframes[n].ddi[i] == p_fp_info->edch_ddi[m])
                    {
                        size = p_fp_info->edch_macd_pdu_size[m];
                        break;
                    }
                }

                if (m == p_fp_info->no_ddi_entries)
                {
                    /* Not found.  Oops */
                    return;
                }

                /* Send MAC-dd PDUs together as one MAC-es PDU */
                send_size = size * subframes[n].number_of_mac_d_pdus[i];

                /* 2 bits spare */
                proto_tree_add_item(subframe_tree, hf_fp_edch_pdu_padding, tvb,
                                    offset + (bit_offset/8),
                                    1, FALSE);
                bit_offset += 2;

                /* TSN */
                tsn = (tvb_get_guint8(tvb, offset + (bit_offset/8)) & 0x3f);
                proto_tree_add_item(subframe_tree, hf_fp_edch_tsn, tvb,
                                    offset + (bit_offset/8),
                                    1, FALSE);
                bit_offset += 6;

                /* PDU */
                if (subframe_tree)
                {
                    ti = proto_tree_add_item(subframe_tree, hf_fp_edch_mac_es_pdu, tvb,
                                             offset + (bit_offset/8),
                                             ((bit_offset % 8) + send_size + 7) / 8,
                                             FALSE);
                    proto_item_append_text(ti, " (%u * %u = %u bits, subframe %d)",
                                           size, subframes[n].number_of_mac_d_pdus[i],
                                           send_size, n);
                }
                bits_in_subframe += send_size;
                mac_d_pdus_in_subframe += subframes[n].number_of_mac_d_pdus[i];

                bit_offset += send_size;

                /* Pad out to next byte */
                if (bit_offset % 8)
                {
                    bit_offset += (8 - (bit_offset % 8));
                }
            }

            if (tree)
            {
                /* Tree should cover entire subframe */
                proto_item_set_len(subframe_ti, bit_offset/8);

                /* Append summary info to subframe label */
                proto_item_append_text(subframe_ti, " (%u bits in %u MAC-d PDUs)",
                                       bits_in_subframe, mac_d_pdus_in_subframe);
            }
            total_pdus += mac_d_pdus_in_subframe;
            total_bits += bits_in_subframe;

            offset += (bit_offset/8);
        }

        /* Report number of subframes in info column */
        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO,
                            " CFN = %03u   (%u bits in %u pdus in %u subframes)",
                            cfn, total_bits, total_pdus, number_of_subframes);
        }

        /* Spare extension and payload CRC (optional) */
        dissect_spare_extension_and_crc(tvb, pinfo, tree,
                                        p_fp_info->dch_crc_present, offset);
    }
}


/**********************************************************/
/* Dissect an HSDSCH channel                              */
/* The data format corresponds to the format              */
/* described in R5 and R6, and frame type 1 in Release 7. */
void dissect_hsdsch_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                 int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        guint8 number_of_pdus;
        guint16 pdu_length;
        guint16 user_buffer_size;

        /**************************************/
        /* HS-DCH data here (type 1 in R7)    */

        /* Frame Seq Nr */
        if ((p_fp_info->release == 6) ||
            (p_fp_info->release == 7))
        {
            guint8 frame_seq_no = (tvb_get_guint8(tvb, offset) & 0xf0) >> 4;
            proto_tree_add_item(tree, hf_fp_frame_seq_nr, tvb, offset, 1, FALSE);

            if (check_col(pinfo->cinfo, COL_INFO))
            {
                col_append_fstr(pinfo->cinfo, COL_INFO, "  seqno=%u", frame_seq_no);
            }
        }

        /* CmCH-PI */
        proto_tree_add_item(tree, hf_fp_cmch_pi, tvb, offset, 1, FALSE);
        offset++;

        /* MAC-d PDU Length (13 bits) */
        pdu_length = (tvb_get_ntohs(tvb, offset) >> 3);
        proto_tree_add_item(tree, hf_fp_mac_d_pdu_len, tvb, offset, 2, FALSE);
        offset += 2;

        if ((p_fp_info->release == 6) ||
            (p_fp_info->release == 7))
        {
            /* Flush bit */
            proto_tree_add_item(tree, hf_fp_flush, tvb, offset-1, 1, FALSE);

            /* FSN/DRT reset bit */
            proto_tree_add_item(tree, hf_fp_fsn_drt_reset, tvb, offset-1, 1, FALSE);
        }


        /* Num of PDU */
        number_of_pdus = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_fp_num_of_pdu, tvb, offset, 1, FALSE);
        offset++;

        /* User buffer size */
        user_buffer_size = tvb_get_ntohs(tvb, offset);
        proto_tree_add_item(tree, hf_fp_user_buffer_size, tvb, offset, 2, FALSE);
        offset += 2;

        /* MAC-d PDUs */
        offset = dissect_macd_pdu_data(tvb, pinfo, tree, offset, pdu_length,
                                       number_of_pdus);

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "  User-Buffer-Size=%u", user_buffer_size);
        }

        /* Extra IEs (if there is room for them) */
        if (((p_fp_info->release == 6) ||
             (p_fp_info->release == 7)) &&
            (tvb_length_remaining(tvb, offset) > 2))
        {
            int n;
            guint8 flags;
            guint8 flag_bytes = 0;

            /* New IE flags */
            do
            {
                proto_item *new_ie_flags_ti;
                proto_tree *new_ie_flags_tree;
                guint ies_found = 0;

                /* Add new IE flags subtree */
                new_ie_flags_ti = proto_tree_add_string_format(tree, hf_fp_hsdsch_new_ie_flags, tvb, offset, 1,
                                                              "", "New IE flags");
                new_ie_flags_tree = proto_item_add_subtree(new_ie_flags_ti, ett_fp_hsdsch_new_ie_flags);

                /* Read next byte */
                flags = tvb_get_guint8(tvb, offset);
                flag_bytes++;

                /* Dissect individual bits */
                for (n=0; n < 8; n++)
                {
                    proto_tree_add_item(new_ie_flags_tree, hf_fp_hsdsch_new_ie_flag[n], tvb, offset, 1, FALSE);
                    if ((flags >> (7-n)) & 0x01)
                    {
                        ies_found++;
                    }
                }
                offset++;

                proto_item_append_text(new_ie_flags_ti, " (%u IEs found)", ies_found);

                /* Last bit set will indicate another flags byte follows... */
            } while (0); /*((flags & 0x01) && (flag_bytes < 31));*/

            if (1) /*(flags & 0x8) */
            {
                /* DRT is shown as mandatory in the diagram (3GPP TS 25.435 V6.3.0),
                   but the description below it states that
                   it should depend upon the first bit.  The detailed description of
                   New IE flags doesn't agree, so treat as mandatory for now... */
                proto_tree_add_item(tree, hf_fp_hsdsch_drt, tvb, offset, 2, FALSE);
                offset += 2;
            }
        }

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}


/******************************************/
/* Dissect an HSDSCH type 2 channel       */
/* (introduced in Release 7)              */
/* N.B. there is currently no support for */
/* frame type 3 (IuR only?)               */
void dissect_hsdsch_type_2_channel_info(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                                        int offset, struct fp_info *p_fp_info)
{
    gboolean is_control_frame;

    /* Header CRC */
    proto_tree_add_item(tree, hf_fp_header_crc, tvb, offset, 1, FALSE);

    /* Frame Type */
    is_control_frame = tvb_get_guint8(tvb, offset) & 0x01;
    proto_tree_add_item(tree, hf_fp_ft, tvb, offset, 1, FALSE);
    offset++;

    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_append_str(pinfo->cinfo, COL_INFO, is_control_frame ? " [Control] " : " [Data] ");
    }

    if (is_control_frame)
    {
        dissect_common_control(tvb, pinfo, tree, offset, p_fp_info);
    }
    else
    {
        guint8 number_of_pdu_blocks;
        gboolean drt_present = FALSE;
        gboolean fach_present = FALSE;
        guint16 user_buffer_size;
        int n;

        #define MAX_PDU_BLOCKS 31
        guint64 lchid[MAX_PDU_BLOCKS];
        guint64 pdu_length[MAX_PDU_BLOCKS];
        guint64 no_of_pdus[MAX_PDU_BLOCKS];

        /********************************/
        /* HS-DCH type 2 data here      */

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_str(pinfo->cinfo, COL_INFO, "(ehs)");
        }

        /* Frame Seq Nr (4 bits) */
        if ((p_fp_info->release == 6) ||
            (p_fp_info->release == 7))
        {
            guint8 frame_seq_no = (tvb_get_guint8(tvb, offset) & 0xf0) >> 4;
            proto_tree_add_item(tree, hf_fp_frame_seq_nr, tvb, offset, 1, FALSE);

            if (check_col(pinfo->cinfo, COL_INFO))
            {
                col_append_fstr(pinfo->cinfo, COL_INFO, "  seqno=%u", frame_seq_no);
            }
        }

        /* CmCH-PI (4 bits) */
        proto_tree_add_item(tree, hf_fp_cmch_pi, tvb, offset, 1, FALSE);
        offset++;

        /* Total number of PDU blocks (5 bits) */
        number_of_pdu_blocks = (tvb_get_guint8(tvb, offset) >> 3);
        proto_tree_add_item(tree, hf_fp_total_pdu_blocks, tvb, offset, 1, FALSE);

        if (p_fp_info->release == 7)
        {
            /* Flush bit */
            proto_tree_add_item(tree, hf_fp_flush, tvb, offset, 1, FALSE);

            /* FSN/DRT reset bit */
            proto_tree_add_item(tree, hf_fp_fsn_drt_reset, tvb, offset, 1, FALSE);

            /* DRT Indicator */
            drt_present = tvb_get_guint8(tvb, offset) & 0x01;
            proto_tree_add_item(tree, hf_fp_drt_indicator, tvb, offset, 1, FALSE);
        }
        offset++;

        /* FACH Indicator flag */
        fach_present = (tvb_get_guint8(tvb, offset) & 0x08) >> 7;
        proto_tree_add_item(tree, hf_fp_fach_indicator, tvb, offset, 1, FALSE);
        offset++;

        /* User buffer size */
        user_buffer_size = tvb_get_ntohs(tvb, offset);
        proto_tree_add_item(tree, hf_fp_user_buffer_size, tvb, offset, 2, FALSE);
        offset += 2;

        if (check_col(pinfo->cinfo, COL_INFO))
        {
            col_append_fstr(pinfo->cinfo, COL_INFO, "  User-Buffer-Size=%u", user_buffer_size);
        }


        /********************************************************************/
        /* Now read number_of_pdu_blocks header entries                     */
        for (n=0; n < number_of_pdu_blocks; n++)
        {
            proto_item *pdu_block_header_ti;
            proto_tree *pdu_block_header_tree;

            /* Add PDU block header subtree */
            pdu_block_header_ti = proto_tree_add_string_format(tree, hf_fp_hsdsch_pdu_block_header,
                                                               tvb, offset, 0,
                                                               "",
                                                               "PDU Block Header");
            pdu_block_header_tree = proto_item_add_subtree(pdu_block_header_ti,
                                                           ett_fp_hsdsch_pdu_block_header);

            /* MAC-d/c PDU length in this block (11 bits) */
            proto_tree_add_bits_ret_val(pdu_block_header_tree, hf_fp_pdu_length_in_block, tvb,
                                        (offset*8) + ((n % 2) ? 4 : 0), 11,
                                        &pdu_length[n], FALSE);
            if ((n % 2) == 0)
                offset++;
            else
                offset += 2;


            /* # PDUs in this block (4 bits) */
            proto_tree_add_bits_ret_val(pdu_block_header_tree, hf_fp_pdus_in_block, tvb,
                                        (offset*8) + ((n % 2) ? 0 : 4), 4,
                                        &no_of_pdus[n], FALSE);
            if ((n % 2) == 0)
                offset++;

            /* Logical channel ID in block (4 bits) */
            proto_tree_add_bits_ret_val(pdu_block_header_tree, hf_fp_lchid, tvb,
                                        (offset*8) + ((n % 2) ? 4 : 0), 4,
                                        &lchid[n], FALSE);
            if ((n % 2) == 1)
                offset++;
            else {
                if (n == (number_of_pdu_blocks-1)) {
                    /* Byte is padded out for last block */
                    offset++;
                }
            }

            /* Append summary to header tree root */
            proto_item_append_text(pdu_block_header_ti,
                                   " (lch:%u, %u pdus of %u bytes)",
                                   (guint16)lchid[n],
                                   (guint16)no_of_pdus[n],
                                   (guint16)pdu_length[n]);
        }


        /**********************************************/
        /* Optional fields indicated by earlier flags */
        if (drt_present)
        {
            /* DRT */
            proto_tree_add_item(tree, hf_fp_drt, tvb, offset, 2, FALSE);
            offset += 2;
        }

        if (fach_present)
        {
            /* H-RNTI: */
            proto_tree_add_item(tree, hf_fp_hrnti, tvb, offset, 2, FALSE);
            offset += 2;

            /* RACH Measurement Result */
            proto_tree_add_item(tree, hf_fp_rach_measurement_result, tvb, offset, 2, FALSE);
            offset++;
        }


        /********************************************************************/
        /* Now read the MAC-d/c PDUs for each block using info from headers */
        for (n=0; n < number_of_pdu_blocks; n++)
        {
            /* Add PDU block header subtree */
            offset = dissect_macd_pdu_data_type_2(tvb, pinfo, tree, offset,
                                                  (guint16)pdu_length[n],
                                                  (guint16)no_of_pdus[n]);
        }

        /* Spare Extension and Payload CRC */
        dissect_spare_extension_and_crc(tvb, pinfo, tree, 1, offset);
    }
}



/*****************************/
/* Main dissection function. */
void dissect_fp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    proto_tree       *fp_tree;
    proto_item       *ti;
    gint             offset = 0;
    struct fp_info   *p_fp_info;

    /* Append this protocol name rather than replace. */
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "FP");

    /* Create fp tree. */
    ti = proto_tree_add_item(tree, proto_fp, tvb, offset, -1, FALSE);
    fp_tree = proto_item_add_subtree(ti, ett_fp);

    /* Look for packet info! */
    p_fp_info = p_get_proto_data(pinfo->fd, proto_fp);

    /* Can't dissect anything without it... */
    if (p_fp_info == NULL)
    {
        proto_item *ti =
            proto_tree_add_text(fp_tree, tvb, offset, -1,
                                "Can't dissect FP frame because no per-frame info was attached!");
        PROTO_ITEM_SET_GENERATED(ti);
        return;
    }

    /* Show channel type in info column, tree */
    if (check_col(pinfo->cinfo, COL_INFO))
    {
        col_add_str(pinfo->cinfo, COL_INFO,
                    val_to_str(p_fp_info->channel,
                               channel_type_vals,
                               "Unknown channel type"));
    }
    proto_item_append_text(ti, " (%s)",
                           val_to_str(p_fp_info->channel,
                                      channel_type_vals,
                                      "Unknown channel type"));

    /* Add channel type as a generated field */
    ti = proto_tree_add_uint(fp_tree, hf_fp_channel_type, tvb, 0, 0, p_fp_info->channel);
    PROTO_ITEM_SET_GENERATED(ti);

    /* Add division type as a generated field */
    if (p_fp_info->release == 7)
    {
        ti = proto_tree_add_uint(fp_tree, hf_fp_division, tvb, 0, 0, p_fp_info->division);
        PROTO_ITEM_SET_GENERATED(ti);
    }

    /* Add link direction as a generated field */
    ti = proto_tree_add_uint(fp_tree, hf_fp_direction, tvb, 0, 0, p_fp_info->is_uplink);
    PROTO_ITEM_SET_GENERATED(ti);

    /* Don't currently handle IuR-specific formats, but its useful to even see
       the channel type and direction */
    if (p_fp_info->iface_type == IuR_Interface)
    {
        return;
    }

    /*************************************/
    /* Dissect according to channel type */
    switch (p_fp_info->channel)
    {
        case CHANNEL_RACH_TDD:
        case CHANNEL_RACH_TDD_128:
        case CHANNEL_RACH_FDD:
             dissect_rach_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_DCH:
            dissect_dch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_FACH_FDD:
        case CHANNEL_FACH_TDD:
            dissect_fach_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_DSCH_FDD:
        case CHANNEL_DSCH_TDD:
            dissect_dsch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_USCH_TDD_128:
        case CHANNEL_USCH_TDD_384:
            dissect_usch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_PCH:
            dissect_pch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_CPCH:
            dissect_cpch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_BCH:
            dissect_bch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_HSDSCH:
            /* Show configured MAC HS-DSCH entity in use */
            if (fp_tree)
            {
                proto_item *entity_ti;
                entity_ti = proto_tree_add_uint(fp_tree, hf_fp_hsdsch_entity,
                                                tvb, 0, 0,
                                                p_fp_info->hsdsch_entity);
                PROTO_ITEM_SET_GENERATED(entity_ti);
            }
            switch (p_fp_info->hsdsch_entity) {
                case entity_not_specified:
                case hs:
                    /* This is the pre-R7 default */
                    dissect_hsdsch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
                    break;
                case ehs:
                    dissect_hsdsch_type_2_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
                    break;
                default:
                    /* TODO: dissector error */
                    break;
            }
            break;
        case CHANNEL_IUR_CPCHF:
            /* TODO: */
            break;
        case CHANNEL_IUR_FACH:
            /* TODO: */
            break;
        case CHANNEL_IUR_DSCH:
            dissect_iur_dsch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;
        case CHANNEL_EDCH:
            dissect_e_dch_channel_info(tvb, pinfo, fp_tree, offset, p_fp_info);
            break;

        default:
            break;
    }
}


void proto_register_fp(void)
{
    static hf_register_info hf[] =
    {
        { &hf_fp_channel_type,
            { "Channel Type",
              "fp.channel-type", FT_UINT8, BASE_HEX, VALS(channel_type_vals), 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_division,
            { "Division",
              "fp.division", FT_UINT8, BASE_HEX, VALS(division_vals), 0x0,
              "Radio division type", HFILL
            }
        },
        { &hf_fp_direction,
            { "Direction",
              "fp.direction", FT_UINT8, BASE_HEX, VALS(direction_vals), 0x0,
              "Link direction", HFILL
            }
        },
        { &hf_fp_header_crc,
            { "Header CRC",
              "fp.header-crc", FT_UINT8, BASE_HEX, NULL, 0xfe,
              NULL, HFILL
            }
        },
        { &hf_fp_ft,
            { "Frame Type",
              "fp.ft", FT_UINT8, BASE_HEX, VALS(data_control_vals), 0x01,
              NULL, HFILL
            }
        },
        { &hf_fp_cfn,
            { "CFN",
              "fp.cfn", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Connection Frame Number", HFILL
            }
        },
        { &hf_fp_pch_cfn,
            { "CFN (PCH)",
              "fp.pch.cfn", FT_UINT16, BASE_DEC, NULL, 0xfff0,
              "PCH Connection Frame Number", HFILL
            }
        },
        { &hf_fp_pch_toa,
            { "ToA (PCH)",
              "fp.pch.toa", FT_INT24, BASE_DEC, NULL, 0x0,
              "PCH Time of Arrival", HFILL
            }
        },
        { &hf_fp_cfn_control,
            { "CFN control",
              "fp.cfn-control", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Connection Frame Number Control", HFILL
            }
        },
        { &hf_fp_toa,
            { "ToA",
              "fp.cfn-control", FT_INT16, BASE_DEC, NULL, 0x0,
              "Time of arrival (units are 125 microseconds)", HFILL
            }
        },
        { &hf_fp_tb,
            { "TB",
              "fp.tb", FT_BYTES, BASE_NONE, NULL, 0x0,
              "Transport Block", HFILL
            }
        },
        { &hf_fp_chan_zero_tbs,
            { "No TBs for channel",
              "fp.channel-with-zero-tbs", FT_UINT32, BASE_DEC, NULL, 0x0,
              "Channel with 0 TBs", HFILL
            }
        },
        { &hf_fp_tfi,
            { "TFI",
              "fp.tfi", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Transport Format Indicator", HFILL
            }
        },
        { &hf_fp_usch_tfi,
            { "TFI",
              "fp.usch.tfi", FT_UINT8, BASE_DEC, NULL, 0x1f,
              "USCH Transport Format Indicator", HFILL
            }
        },
        { &hf_fp_cpch_tfi,
            { "TFI",
              "fp.cpch.tfi", FT_UINT8, BASE_DEC, NULL, 0x1f,
              "CPCH Transport Format Indicator", HFILL
            }
        },
        { &hf_fp_propagation_delay,
            { "Propagation Delay",
              "fp.propagation-delay", FT_UINT8, BASE_DEC, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_dch_control_frame_type,
            { "Control Frame Type",
              "fp.dch.control.frame-type", FT_UINT8, BASE_HEX, VALS(dch_control_frame_type_vals), 0x0,
              "DCH Control Frame Type", HFILL
            }
        },
        { &hf_fp_dch_rx_timing_deviation,
            { "Rx Timing Deviation",
              "fp.dch.control.rx-timing-deviation", FT_UINT8, BASE_DEC, 0, 0x0,
              "DCH Rx Timing Deviation", HFILL
            }
        },
        { &hf_fp_quality_estimate,
            { "Quality Estimate",
              "fp.dch.quality-estimate", FT_UINT8, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_payload_crc,
            { "Payload CRC",
              "fp.payload-crc", FT_UINT16, BASE_HEX, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_common_control_frame_type,
            { "Control Frame Type",
              "fp.common.control.frame-type", FT_UINT8, BASE_HEX, VALS(common_control_frame_type_vals), 0x0,
              "Common Control Frame Type", HFILL
            }
        },
        { &hf_fp_crci[0],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x80,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[1],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x40,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[2],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x20,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[3],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x10,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[4],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x08,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[5],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x04,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[6],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x02,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_crci[7],
            { "CRCI",
              "fp.crci", FT_UINT8, BASE_HEX, VALS(crci_vals), 0x01,
              "CRC correctness indicator", HFILL
            }
        },
        { &hf_fp_received_sync_ul_timing_deviation,
            { "Received SYNC UL Timing Deviation",
              "fp.rx-sync-ul-timing-deviation", FT_UINT8, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_pch_pi,
            { "Paging Indication",
              "fp.pch.pi", FT_UINT8, BASE_DEC, VALS(paging_indication_vals), 0x01,
              "Indicates if the PI Bitmap is present", HFILL
            }
        },
        { &hf_fp_pch_tfi,
            { "TFI",
              "fp.pch.tfi", FT_UINT8, BASE_DEC, 0, 0x1f,
              "PCH Transport Format Indicator", HFILL
            }
        },
        { &hf_fp_fach_tfi,
            { "TFI",
              "fp.fach.tfi", FT_UINT8, BASE_DEC, 0, 0x1f,
              "FACH Transport Format Indicator", HFILL
            }
        },
        { &hf_fp_transmit_power_level,
            { "Transmit Power Level",
              "fp.transmit-power-level", FT_FLOAT, BASE_NONE, 0, 0x0,
              "Transmit Power Level (dB)", HFILL
            }
        },
        { &hf_fp_pdsch_set_id,
            { "PDSCH Set Id",
              "fp.pdsch-set-id", FT_UINT8, BASE_DEC, 0, 0x0,
              "A pointer to the PDSCH Set which shall be used to transmit", HFILL
            }
        },
        { &hf_fp_paging_indication_bitmap,
            { "Paging Indications bitmap",
              "fp.pch.pi-bitmap", FT_NONE, BASE_NONE, NULL, 0x0,
              "Paging Indication bitmap", HFILL
            }
        },
        { &hf_fp_rx_timing_deviation,
            { "Rx Timing Deviation",
              "fp.common.control.rx-timing-deviation", FT_UINT8, BASE_DEC, 0, 0x0,
              "Common Rx Timing Deviation", HFILL
            }
        },
        { &hf_fp_dch_e_rucch_flag,
            { "E-RUCCH Flag",
              "fp.common.control.e-rucch-flag", FT_UINT8, BASE_DEC, VALS(e_rucch_flag_vals), 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_edch_header_crc,
            { "E-DCH Header CRC",
              "fp.edch.header-crc", FT_UINT16, BASE_HEX, 0, 0xfef,
              NULL, HFILL
            }
        },
        { &hf_fp_edch_fsn,
            { "FSN",
              "fp.edch.fsn", FT_UINT8, BASE_DEC, 0, 0x0f,
              "E-DCH Frame Sequence Number", HFILL
            }
        },
        { &hf_fp_edch_number_of_subframes,
            { "No of subframes",
              "fp.edch.no-of-subframes", FT_UINT8, BASE_DEC, 0, 0x0f,
              "E-DCH Number of subframes", HFILL
            }
        },
        { &hf_fp_edch_harq_retransmissions,
            { "No of HARQ Retransmissions",
              "fp.edch.no-of-harq-retransmissions", FT_UINT8, BASE_DEC, 0, 0x78,
              "E-DCH Number of HARQ retransmissions", HFILL
            }
        },
        { &hf_fp_edch_subframe_number,
            { "Subframe number",
              "fp.edch.subframe-number", FT_UINT8, BASE_DEC, 0, 0x07,
              "E-DCH Subframe number", HFILL
            }
        },
        { &hf_fp_edch_number_of_mac_es_pdus,
            { "Number of Mac-es PDUs",
              "fp.edch.number-of-mac-es-pdus", FT_UINT8, BASE_DEC, 0, 0xf0,
              NULL, HFILL
            }
        },
        { &hf_fp_edch_ddi,
            { "DDI",
              "fp.edch.ddi", FT_UINT8, BASE_DEC, 0, 0x0,
              "E-DCH Data Description Indicator", HFILL
            }
        },
        { &hf_fp_edch_subframe,
            { "Subframe",
              "fp.edch.subframe", FT_STRING, BASE_NONE, NULL, 0x0,
              "EDCH Subframe", HFILL
            }
        },
        { &hf_fp_edch_subframe_header,
            { "Subframe header",
              "fp.edch.subframe-header", FT_STRING, BASE_NONE, NULL, 0x0,
              "EDCH Subframe header", HFILL
            }
        },
        { &hf_fp_edch_number_of_mac_d_pdus,
            { "Number of Mac-d PDUs",
              "fp.edch.number-of-mac-d-pdus", FT_UINT8, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_edch_pdu_padding,
            { "Padding",
              "fp.edch-data-padding", FT_UINT8, BASE_DEC, 0, 0xc0,
              "E-DCH padding before PDU", HFILL
            }
        },
        { &hf_fp_edch_tsn,
            { "TSN",
              "fp.edch-tsn", FT_UINT8, BASE_DEC, 0, 0x3f,
              "E-DCH Transmission Sequence Number", HFILL
            }
        },
        { &hf_fp_edch_mac_es_pdu,
            { "MAC-es PDU",
              "fp.edch.mac-es-pdu", FT_NONE, BASE_NONE, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_frame_seq_nr,
            { "Frame Seq Nr",
              "fp.frame-seq-nr", FT_UINT8, BASE_DEC, 0, 0xf0,
              "Frame Sequence Number", HFILL
            }
        },
        { &hf_fp_hsdsch_pdu_block_header,
            { "PDU block header",
              "fp.hsdsch.pdu-block-header", FT_STRING, BASE_NONE, NULL, 0x0,
              "HS-DSCH type 2 PDU block header", HFILL
            }
        },
        { &hf_fp_hsdsch_pdu_block,
            { "PDU block",
              "fp.hsdsch.pdu-block", FT_STRING, BASE_NONE, NULL, 0x0,
              "HS-DSCH type 2 PDU block data", HFILL
            }
        },
        { &hf_fp_flush,
            { "Flush",
              "fp.flush", FT_UINT8, BASE_DEC, 0, 0x04,
              "Whether all PDUs for this priority queue should be removed", HFILL
            }
        },
        { &hf_fp_fsn_drt_reset,
            { "FSN-DRT reset",
              "fp.fsn-drt-reset", FT_UINT8, BASE_DEC, 0, 0x02,
              "FSN/DRT Reset Flag", HFILL
            }
        },
        { &hf_fp_drt_indicator,
            { "DRT Indicator",
              "fp.drt-indicator", FT_UINT8, BASE_DEC, 0, 0x01,
              NULL, HFILL
            }
        },
        { &hf_fp_fach_indicator,
            { "FACH Indicator",
              "fp.fach-indicator", FT_UINT8, BASE_DEC, 0, 0x80,
              NULL, HFILL
            }
        },
        { &hf_fp_total_pdu_blocks,
            { "PDU Blocks",
              "fp.pdu_blocks", FT_UINT8, BASE_DEC, 0, 0xf8,
              "Total number of PDU blocks", HFILL
            }
        },
        { &hf_fp_drt,
            { "DRT",
              "fp.drt", FT_UINT16, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_hrnti,
            { "HRNTI",
              "fp.hrnti", FT_UINT16, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_measurement_result,
            { "RACH Measurement Result",
              "fp.rach-measurement-result", FT_UINT16, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_lchid,
            { "Logical Channel ID",
              "fp.lchid", FT_UINT8, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_pdu_length_in_block,
            { "PDU length in block",
              "fp.pdu-length-in-block", FT_UINT8, BASE_DEC, 0, 0x0,
              "Length of each PDU in this block in bytes", HFILL
            }
        },
        { &hf_fp_pdus_in_block,
            { "PDUs in block",
              "fp.no-pdus-in-block", FT_UINT8, BASE_DEC, 0, 0x0,
              "Number of PDUs in block", HFILL
            }
        },
        { &hf_fp_cmch_pi,
            { "CmCH-PI",
              "fp.cmch-pi", FT_UINT8, BASE_DEC, 0, 0x0f,
              "Common Transport Channel Priority Indicator", HFILL
            }
        },
        { &hf_fp_user_buffer_size,
            { "User buffer size",
              "fp.user-buffer-size", FT_UINT16, BASE_DEC, 0, 0x0,
              "User buffer size in octets", HFILL
            }
        },
        { &hf_fp_hsdsch_credits,
            { "HS-DSCH Credits",
              "fp.hsdsch-credits", FT_UINT16, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_max_macd_pdu_len,
            { "Max MAC-d PDU Length",
              "fp.hsdsch.max-macd-pdu-len", FT_UINT16, BASE_DEC, 0, 0xfff8,
              "Maximum MAC-d PDU Length in bits", HFILL
            }
        },
        { &hf_fp_hsdsch_max_macdc_pdu_len,
            { "Max MAC-d/c PDU Length",
              "fp.hsdsch.max-macdc-pdu-len", FT_UINT16, BASE_DEC, 0, 0x07ff,
              "Maximum MAC-d/c PDU Length in bits", HFILL
            }
        },
        { &hf_fp_hsdsch_interval,
            { "HS-DSCH Interval in milliseconds",
              "fp.hsdsch-interval", FT_UINT8, BASE_DEC, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_calculated_rate,
            { "Calculated rate allocation (bps)",
              "fp.hsdsch-calculated-rate", FT_UINT32, BASE_DEC, 0, 0x0,
              "Calculated rate RNC is allowed to send in bps", HFILL
            }
        },
        { &hf_fp_hsdsch_unlimited_rate,
            { "Unlimited rate",
              "fp.hsdsch-unlimited-rate", FT_NONE, BASE_NONE, 0, 0x0,
              "No restriction on rate at which date may be sent", HFILL
            }
        },
        { &hf_fp_hsdsch_repetition_period,
            { "HS-DSCH Repetition Period",
              "fp.hsdsch-repetition-period", FT_UINT8, BASE_DEC, 0, 0x0,
              "HS-DSCH Repetition Period in milliseconds", HFILL
            }
        },
        { &hf_fp_hsdsch_data_padding,
            { "Padding",
              "fp.hsdsch-data-padding", FT_UINT8, BASE_DEC, 0, 0xf0,
              "HS-DSCH Repetition Period in milliseconds", HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flags,
            { "New IEs flags",
              "fp.hsdsch.new-ie-flags", FT_STRING, BASE_NONE, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[0],
            { "DRT IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x80,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[1],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x40,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[2],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x20,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[3],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x10,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[4],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x08,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[5],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x04,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[6],
            { "New IE present",
              "fp.hsdsch.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x02,
              NULL, HFILL
            }
        },
        { &hf_fp_hsdsch_new_ie_flag[7],
            { "Another new IE flags byte",
              "fp.hsdsch.new-ie-flags-byte", FT_UINT8, BASE_DEC, 0, 0x01,
              "Another new IE flagsbyte", HFILL
            }
        },
        { &hf_fp_hsdsch_drt,
            { "DRT",
              "fp.hsdsch.drt", FT_UINT8, BASE_DEC, 0, 0xf0,
              "Delay Reference Time", HFILL
            }
        },
        { &hf_fp_hsdsch_entity,
            { "HS-DSCH Entity",
              "fp.hsdsch.entity", FT_UINT8, BASE_DEC, VALS(hsdshc_mac_entity_vals), 0x0,
              "Type of MAC entity for this HS-DSCH channel", HFILL
            }
        },
        { &hf_fp_timing_advance,
            { "Timing advance",
              "fp.timing-advance", FT_UINT8, BASE_DEC, 0, 0x3f,
              "Timing advance in chips", HFILL
            }
        },
        { &hf_fp_num_of_pdu,
            { "Number of PDUs",
              "fp.hsdsch.num-of-pdu", FT_UINT8, BASE_DEC, 0, 0x0,
              "Number of PDUs in the payload", HFILL
            }
        },
        { &hf_fp_mac_d_pdu_len,
            { "MAC-d PDU Length",
              "fp.hsdsch.mac-d-pdu-len", FT_UINT16, BASE_DEC, 0, 0xfff8,
              "MAC-d PDU Length in bits", HFILL
            }
        },
        { &hf_fp_mac_d_pdu,
            { "MAC-d PDU",
              "fp.mac-d-pdu", FT_BYTES, BASE_NONE, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_data,
            { "Data",
              "fp.data", FT_BYTES, BASE_NONE, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_crcis,
            { "CRCIs",
              "fp.crcis", FT_BYTES, BASE_NONE, NULL, 0x0,
              "CRC Indicators for uplink TBs", HFILL
            }
        },
        { &hf_fp_t1,
            { "T1",
              "fp.t1", FT_UINT24, BASE_DEC, NULL, 0x0,
              "RNC frame number indicating time it sends frame", HFILL
            }
        },
        { &hf_fp_t2,
            { "T2",
              "fp.t2", FT_UINT24, BASE_DEC, NULL, 0x0,
              "NodeB frame number indicating time it received DL Sync", HFILL
            }
        },
        { &hf_fp_t3,
            { "T3",
              "fp.t3", FT_UINT24, BASE_DEC, NULL, 0x0,
              "NodeB frame number indicating time it sends frame", HFILL
            }
        },
        { &hf_fp_ul_sir_target,
            { "UL_SIR_TARGET",
              "fp.ul-sir-target", FT_FLOAT, BASE_NONE, 0, 0x0,
              "Value (in dB) of the SIR target to be used by the UL inner loop power control", HFILL
            }
        },
        { &hf_fp_pusch_set_id,
            { "PUSCH Set Id",
              "fp.pusch-set-id", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Identifies PUSCH Set from those configured in NodeB", HFILL
            }
        },
        { &hf_fp_activation_cfn,
            { "Activation CFN",
              "fp.activation-cfn", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Activation Connection Frame Number", HFILL
            }
        },
        { &hf_fp_duration,
            { "Duration (ms)",
              "fp.pusch-set-id", FT_UINT8, BASE_DEC, NULL, 0x0,
              "Duration of the activation period of the PUSCH Set", HFILL
            }
        },
        { &hf_fp_power_offset,
            { "Power offset",
              "fp.power-offset", FT_FLOAT, BASE_NONE, NULL, 0x0,
              "Power offset (in dB)", HFILL
            }
        },
        { &hf_fp_code_number,
            { "Code number",
              "fp.code-number", FT_UINT8, BASE_DEC, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_spreading_factor,
            { "Spreading factor",
              "fp.spreading-factor", FT_UINT8, BASE_DEC, VALS(spreading_factor_vals), 0xf0,
              NULL, HFILL
            }
        },
        { &hf_fp_mc_info,
            { "MC info",
              "fp.mc-info", FT_UINT8, BASE_DEC, NULL, 0x0e,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_new_ie_flags,
            { "New IEs flags",
              "fp.rach.new-ie-flags", FT_STRING, BASE_NONE, 0, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[0],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x80,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[1],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x40,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[2],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x20,
              "New IE present (unused)", HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[3],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x10,
              "New IE present (unused)", HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[4],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x08,
              "New IE present (unused)", HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[5],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x04,
              "New IE present (unused)", HFILL
            }
        },
        { &hf_fp_rach_new_ie_flag_unused[6],
            { "New IE present",
              "fp.rach.new-ie-flag", FT_UINT8, BASE_DEC, 0, 0x02,
              "New IE present (unused)", HFILL
            }
        },
        { &hf_fp_rach_cell_portion_id_present,
            { "Cell portion ID present",
              "fp.rach.cell-portion-id-present", FT_UINT8, BASE_DEC, 0, 0x01,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_angle_of_arrival_present,
            { "Angle of arrival present",
              "fp.rach.angle-of-arrival-present", FT_UINT8, BASE_DEC, 0, 0x01,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_ext_propagation_delay_present,
            { "Ext Propagation Delay Present",
              "fp.rach.ext-propagation-delay-present", FT_UINT8, BASE_DEC, 0, 0x02,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_ext_rx_sync_ul_timing_deviation_present,
            { "Ext Received Sync UL Timing Deviation present",
              "fp.rach.ext-rx-sync-ul-timing-deviation-present", FT_UINT8, BASE_DEC, 0, 0x02,
              NULL, HFILL
            }
        },
        { &hf_fp_rach_ext_rx_timing_deviation_present,
            { "Ext Rx Timing Deviation present",
              "fp.rach.ext-rx-timing-deviation-present", FT_UINT8, BASE_DEC, 0, 0x01,
              NULL, HFILL
            }
        },
        { &hf_fp_cell_portion_id,
            { "Cell Portion ID",
              "fp.cell-portion-id", FT_UINT8, BASE_DEC, NULL, 0x3f,
              NULL, HFILL
            }
        },
        { &hf_fp_ext_propagation_delay,
            { "Ext Propagation Delay",
              "fp.ext-propagation-delay", FT_UINT16, BASE_DEC, NULL, 0x03ff,
              NULL, HFILL
            }
        },
        { &hf_fp_angle_of_arrival,
            { "Angle of Arrival",
              "fp.angle-of-arrival", FT_UINT16, BASE_DEC, NULL, 0x03ff,
              NULL, HFILL
            }
        },
        { &hf_fp_ext_received_sync_ul_timing_deviation,
            { "Ext Received SYNC UL Timing Deviation",
              "fp.ext-received-sync-ul-timing-deviation", FT_UINT16, BASE_DEC, NULL, 0x1fff,
              NULL, HFILL
            }
        },


        { &hf_fp_radio_interface_parameter_update_flag[0],
            { "CFN valid",
              "fp.radio-interface-param.cfn-valid", FT_UINT16, BASE_DEC, 0, 0x0001,
              NULL, HFILL
            }
        },
        { &hf_fp_radio_interface_parameter_update_flag[1],
            { "TPC PO valid",
              "fp.radio-interface-param.tpc-po-valid", FT_UINT16, BASE_DEC, 0, 0x0002,
              NULL, HFILL
            }
        },
        { &hf_fp_radio_interface_parameter_update_flag[2],
            { "DPC mode valid",
              "fp.radio-interface-param.dpc-mode-valid", FT_UINT16, BASE_DEC, 0, 0x0004,
              NULL, HFILL
            }
        },
        { &hf_fp_radio_interface_parameter_update_flag[3],
            { "RL sets indicator valid",
              "fp.radio-interface_param.rl-sets-indicator-valid", FT_UINT16, BASE_DEC, 0, 0x0020,
              NULL, HFILL
            }
        },
        { &hf_fp_radio_interface_parameter_update_flag[4],
            { "MAX_UE_TX_POW valid",
              "fp.radio-interface-param.max-ue-tx-pow-valid", FT_UINT16, BASE_DEC, 0, 0x0040,
              "MAX UE TX POW valid", HFILL
            }
        },
        { &hf_fp_dpc_mode,
            { "DPC Mode",
              "fp.dpc-mode", FT_UINT8, BASE_DEC, NULL, 0x20,
              "DPC Mode to be applied in the uplink", HFILL
            }
        },
        { &hf_fp_tpc_po,
            { "TPC PO",
              "fp.tpc-po", FT_UINT8, BASE_DEC, NULL, 0x1f,
              NULL, HFILL
            }
        },
        { &hf_fp_multiple_rl_set_indicator,
            { "Multiple RL sets indicator",
              "fp.multiple-rl-sets-indicator", FT_UINT8, BASE_DEC, NULL, 0x80,
              NULL, HFILL
            }
        },
        { &hf_fp_max_ue_tx_pow,
            { "MAX_UE_TX_POW",
              "fp.max-ue-tx-pow", FT_INT8, BASE_DEC, NULL, 0x0,
              "Max UE TX POW (dBm)", HFILL
            }
        },
        { &hf_fp_congestion_status,
            { "Congestion Status",
              "fp.congestion-status", FT_UINT8, BASE_DEC, VALS(congestion_status_vals), 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_e_rucch_present,
            { "E-RUCCH Present",
              "fp.erucch-present", FT_UINT8, BASE_DEC, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_extended_bits_present,
            { "Extended Bits Present",
              "fp.extended-bits-present", FT_UINT8, BASE_DEC, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_extended_bits,
            { "Extended Bits",
              "fp.extended-bits", FT_UINT8, BASE_HEX, NULL, 0x0,
              NULL, HFILL
            }
        },
        { &hf_fp_spare_extension,
            { "Spare Extension",
              "fp.spare-extension", FT_NONE, BASE_NONE, NULL, 0x0,
              NULL, HFILL
            }
        },

    };


    static gint *ett[] =
    {
        &ett_fp,
        &ett_fp_data,
        &ett_fp_crcis,
        &ett_fp_edch_subframe_header,
        &ett_fp_edch_subframe,
        &ett_fp_hsdsch_new_ie_flags,
        &ett_fp_rach_new_ie_flags,
        &ett_fp_hsdsch_pdu_block_header
    };

    /* Register protocol. */
    proto_fp = proto_register_protocol("FP", "FP", "fp");
    proto_register_field_array(proto_fp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    /* Allow other dissectors to find this one by name. */
    register_dissector("fp", dissect_fp, proto_fp);
}


void proto_reg_handoff_fp(void)
{
}

