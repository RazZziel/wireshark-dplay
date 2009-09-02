/* packet-kdp.c
 * Routines for KDP (Kontiki Delivery Protocol) packet disassembly
 *
 * $Id$
 *
 * Copyright (c) 2008 by Kontiki Inc.
 *                    Wade Hennessey <wade@kontiki.com>
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
# include "config.h"
#endif

#include <epan/packet.h>
#include <glib.h>

#define KDP_PORT 19948
#define BUFFER_SIZE 80
static int proto_kdp = -1;
static gint ett_kdp = -1;
static gint ett_kdp_flags = -1;

static int hf_kdp_version = -1;
static int hf_kdp_headerlen = -1;
static int hf_kdp_flags = -1;
static int hf_kdp_errors = -1;
static int hf_kdp_destflowid = -1;
static int hf_kdp_srcflowid = -1;
static int hf_kdp_sequence = -1;
static int hf_kdp_ack = -1;
static int hf_kdp_maxsegmentsize = -1;
static int hf_kdp_optionnumber = -1;
static int hf_kdp_optionlen = -1;
static int hf_kdp_option1 = -1;
static int hf_kdp_option2 = -1;
static int hf_kdp_fragment = -1;
static int hf_kdp_fragtotal = -1;
static int hf_kdp_body = -1;
static int hf_kdp_xml_body = -1;

#define KDP_DROP_FLAG (1 << 0)
#define KDP_SYN_FLAG  (1 << 1)
#define KDP_ACK_FLAG  (1 << 2)
#define KDP_RST_FLAG  (1 << 3)
#define KDP_BCST_FLAG (1 << 4)
#define KDP_DUP_FLAG  (1 << 5)

static int hf_kdp_drop_flag = -1;
static int hf_kdp_syn_flag = -1;
static int hf_kdp_ack_flag = -1;
static int hf_kdp_rst_flag = -1;
static int hf_kdp_bcst_flag = -1;
static int hf_kdp_dup_flag = -1;

static void dissect_kdp(tvbuff_t *tvb,
			packet_info *pinfo,
			proto_tree *tree) {
  guint body_len;
  guint8 version = 0;
  guint8 header_len = 0;
  guint8 packet_flags = 0;
  guint8 packet_errors = 0;
  guint32 sequence_number = G_MAXUINT32;
  guint32 ack_number = G_MAXUINT32; 
  guint32 src_flowid = G_MAXUINT32; 
  int offset;

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "KDP");
  col_clear(pinfo->cinfo, COL_INFO);
  if (tree) {
    proto_item *ti;
    proto_tree *kdp_tree, *flags_tree;
    ti = NULL;
    kdp_tree = NULL;
    flags_tree = NULL;

    ti = proto_tree_add_item(tree, proto_kdp, tvb, 0, -1, FALSE);
    kdp_tree = proto_item_add_subtree(ti, ett_kdp);

    version = tvb_get_guint8(tvb, 0);
    if (version != 2) {
      /* Version other than 2 is really SDDP in UDP */
      proto_tree_add_item(kdp_tree, hf_kdp_version, tvb, 0, 1, FALSE);
      proto_tree_add_item(kdp_tree, hf_kdp_xml_body, tvb, 0, -1, FALSE);
    } else {
      header_len = tvb_get_guint8(tvb, 1) * 4;
      body_len = tvb_reported_length(tvb);
      if (header_len > body_len) {
	body_len = 0;		/* malformed packet */
      } else {
	body_len = body_len - header_len;
      }
      packet_flags = tvb_get_guint8(tvb, 2);
      packet_errors = tvb_get_guint8(tvb, 3);
      proto_tree_add_item(kdp_tree, hf_kdp_version, tvb, 0, 1, FALSE);
      proto_tree_add_item(kdp_tree, hf_kdp_headerlen, tvb, 1, 1, FALSE);
      ti = proto_tree_add_item(kdp_tree, hf_kdp_flags, tvb, 2, 1, FALSE);
      flags_tree = proto_item_add_subtree(ti, ett_kdp_flags);

      proto_tree_add_item(flags_tree, hf_kdp_drop_flag, tvb, 2, 1, FALSE);
      proto_tree_add_item(flags_tree, hf_kdp_syn_flag, tvb, 2, 1, FALSE);
      proto_tree_add_item(flags_tree, hf_kdp_ack_flag, tvb, 2, 1, FALSE);
      proto_tree_add_item(flags_tree, hf_kdp_rst_flag, tvb, 2, 1, FALSE);
      proto_tree_add_item(flags_tree, hf_kdp_bcst_flag, tvb, 2, 1, FALSE);
      proto_tree_add_item(flags_tree, hf_kdp_dup_flag, tvb, 2, 1, FALSE);

      proto_tree_add_item(kdp_tree, hf_kdp_errors, tvb, 3, 1, FALSE);  

      if (header_len > 4) {
	offset = 4;
	if (packet_flags & KDP_ACK_FLAG) {
	  proto_tree_add_item(kdp_tree, hf_kdp_destflowid, tvb, offset, 4, FALSE);
	  offset = offset + 4;
	}

	if (packet_flags & (KDP_SYN_FLAG | KDP_BCST_FLAG)) {
	  proto_tree_add_item(kdp_tree, hf_kdp_srcflowid, tvb, offset, 4, FALSE);
	  src_flowid = tvb_get_ntohl(tvb, offset);
	  offset = offset + 4;
	}

	proto_tree_add_item(kdp_tree, hf_kdp_sequence, tvb, offset, 4, FALSE);
	sequence_number = tvb_get_ntohl(tvb, offset);
	offset = offset + 4;

	if (packet_flags & KDP_ACK_FLAG) {
	  proto_tree_add_item(kdp_tree, hf_kdp_ack, tvb, offset, 4, FALSE);
	  ack_number = tvb_get_ntohl(tvb, offset);
	  offset = offset + 4;
	}
	if (packet_flags & KDP_SYN_FLAG) {
	  proto_tree_add_item(kdp_tree,
			      hf_kdp_maxsegmentsize, tvb, offset, 4, FALSE);
	  offset = offset + 4;
	}

	while (offset < ((body_len > 0) ? header_len - 4 : header_len)) {
	  guint8 option_number;

	  option_number = tvb_get_guint8(tvb, offset);
      
	  proto_tree_add_item(kdp_tree,
			      hf_kdp_optionnumber, tvb, offset, 1, FALSE);
	  offset = offset + 1;
	  if (option_number > 0) {
	    proto_tree_add_item(kdp_tree,
				hf_kdp_optionlen, tvb, offset, 1, FALSE);
	    offset = offset + 1;
	  }

	  switch (option_number) {
	  case 0:
	    break;
	  case 1:
	    proto_tree_add_item(kdp_tree, hf_kdp_option1, tvb, offset, 2, FALSE);
	    offset = offset + 2;
	    break;
	  case 2:
	    proto_tree_add_item(kdp_tree, hf_kdp_option2, tvb, offset, 2, FALSE);
	    offset = offset + 2;
	    break;
	  default: body_len = 0; /* Invalid option - ignore rest of packet */
	  }
	}
      
	if (body_len > 0) {
	  proto_tree_add_item(kdp_tree, hf_kdp_fragment, tvb, offset, 2, FALSE);
	  offset = offset + 2;

	  proto_tree_add_item(kdp_tree, hf_kdp_fragtotal, tvb, offset, 2, FALSE);
	  offset = offset + 2;

	  proto_tree_add_item(kdp_tree, hf_kdp_body, tvb, offset, -1, FALSE);
	}
      }
    }
  }
  /* Now that we know sequence number and optional ack number, we can
     print more detailed summary info */
  if (check_col(pinfo->cinfo, COL_INFO)) {
    if (version != 2) {
      col_add_fstr(pinfo->cinfo, COL_INFO, "SDDP message");
    } else {
      char ack_string[BUFFER_SIZE];
      char seq_num_string[BUFFER_SIZE];
      char src_flowid_string[BUFFER_SIZE];

      if (packet_flags & KDP_ACK_FLAG) {
	g_snprintf(ack_string, sizeof(ack_string), "ACK=%x ", ack_number);
      } else {
	ack_string[0] = '\0';
      }
      if (header_len > 4) {
	g_snprintf(seq_num_string, sizeof(seq_num_string), "SEQ=%x ", sequence_number);
      } else {
	seq_num_string[0] = '\0';
      }
      if (packet_flags & (KDP_SYN_FLAG | KDP_BCST_FLAG)) {
	g_snprintf(src_flowid_string, sizeof(src_flowid_string), "SRC_FLOWID=%x ", src_flowid);
      } else {
	src_flowid_string[0] = '\0';
      }
      col_add_fstr(pinfo->cinfo, COL_INFO, "%s%s%s%s%s%s%s%serrors=%d",
		   ((packet_flags & KDP_DROP_FLAG) ? "DROP " : ""),
		   ((packet_flags & KDP_SYN_FLAG) ? "SYN " : ""),
		   ((packet_flags & KDP_RST_FLAG) ? "RST " : ""),
		   ((packet_flags & KDP_BCST_FLAG) ? "BCST " : ""),
		   ((packet_flags & KDP_DUP_FLAG) ? "DUP " : ""),
		   ack_string,
		   seq_num_string,
		   src_flowid_string,
		   packet_errors);
    }
  }
}

void proto_register_kdp(void) {

  static hf_register_info hf[] = {
    { &hf_kdp_version,
      {"KDP version", "kdp.version",
       FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_headerlen,
      {"KDP header len", "kdp.headerlen",
       FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_flags,
      {"KDP flags", "kdp.flags",
       FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_drop_flag,
      {"KDP DROP Flag", "kdp.flags.drop",
       FT_BOOLEAN, 8, NULL, KDP_DROP_FLAG, NULL, HFILL}
    },
    { &hf_kdp_syn_flag,
      {"KDP SYN Flag", "kdp.flags.syn",
       FT_BOOLEAN, 8, NULL, KDP_SYN_FLAG, NULL, HFILL}
    },
    { &hf_kdp_ack_flag,
      {"KDP ACK Flag", "kdp.flags.ack",
       FT_BOOLEAN, 8, NULL, KDP_ACK_FLAG, NULL, HFILL}
    },
    { &hf_kdp_rst_flag,
      {"KDP RST Flag", "kdp.flags.rst",
       FT_BOOLEAN, 8, NULL, KDP_RST_FLAG, NULL, HFILL}
    },
    { &hf_kdp_bcst_flag,
      {"KDP BCST Flag", "kdp.flags.bcst",
       FT_BOOLEAN, 8, NULL, KDP_BCST_FLAG, NULL, HFILL}
    },
    { &hf_kdp_dup_flag,
      {"KDP DUP Flag", "kdp.flags.dup",
       FT_BOOLEAN, 8, NULL, KDP_DUP_FLAG, NULL, HFILL}
    },
    { &hf_kdp_errors,
      {"KDP errors", "kdp.errors",
       FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_destflowid,
      { "DestFlowID", "kdp.destflowid",
	FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_srcflowid,
      { "SrcFlowID", "kdp.srcflowid",
	FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_sequence,
      { "Sequence", "kdp.sequence",
	FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_ack,
      { "Ack", "kdp.ack",
	FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_maxsegmentsize,
      { "MaxSegmentSize", "kdp.maxsegmentsize",
	FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_optionnumber,
      { "Option Number", "kdp.optionnumber",
	FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_optionlen,
      { "Option Len", "kdp.option",
	FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_option1,
      { "Option1 - Max Window", "kdp.option1",
	FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_option2,
      { "Option2 - TCP Fraction", "kdp.option2",
	FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_fragment,
      { "Fragment", "kdp.fragment",
	FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_fragtotal,
      { "FragTotal", "kdp.fragtotal",
	FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_body,
      { "Encrypted Body", "kdp.body",
	FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL}
    },
    { &hf_kdp_xml_body,
      { "XML Body", "kdp.body",
	FT_STRING, BASE_NONE, NULL, 0x0, NULL, HFILL}
    }
  };

  /* Setup protocol subtree array */
  static gint *ett[] = {
    &ett_kdp, &ett_kdp_flags
  };

  proto_kdp = proto_register_protocol("Kontiki Delivery Protocol", /* name */
                                      "KDP",		/* short name */
                                      "kdp");		/* abbrev */
  proto_register_field_array(proto_kdp, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_kdp(void) {
  dissector_handle_t kdp_handle;
  kdp_handle = create_dissector_handle(dissect_kdp, proto_kdp);
  dissector_add("udp.port", KDP_PORT, kdp_handle);
}

