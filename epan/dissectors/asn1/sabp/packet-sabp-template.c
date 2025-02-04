/* packet-sabp-template.c
 * Routines for UTRAN Iu-BC Interface: Service Area Broadcast Protocol (SABP) packet dissection
 * Copyright 2007, Tomas Kukosa <tomas.kukosa@siemens.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Ref: 3GPP TS 25.419 version  V9.0.0 (2009-12)
 */

#include "config.h"

#include <epan/packet.h>

#include <epan/asn1.h>

#include "packet-tcp.h"
#include "packet-per.h"
#include "packet-e212.h"
#include "packet-gsm_map.h"
#include "packet-gsm_sms.h"
#include <epan/sctpppids.h>
#include "packet-cell_broadcast.h"

#define PNAME  "UTRAN IuBC interface SABP signaling"
#define PSNAME "SABP"
#define PFNAME "sabp"

#include "packet-sabp-val.h"

void proto_register_sabp(void);
void proto_reg_handoff_sabp(void);

/* Initialize the protocol and registered fields */
static int proto_sabp = -1;

static int hf_sabp_no_of_pages = -1;
static int hf_sabp_cb_inf_len = -1;
static int hf_sabp_cb_msg_inf_page = -1;
static int hf_sabp_cbs_page_content = -1;
#include "packet-sabp-hf.c"

/* Initialize the subtree pointers */
static int ett_sabp = -1;
static int ett_sabp_e212 = -1;
static int ett_sabp_cbs_data_coding = -1;
static int ett_sabp_bcast_msg = -1;
static int ett_sabp_cbs_serial_number = -1;
static int ett_sabp_cbs_new_serial_number = -1;
static int ett_sabp_cbs_page = -1;
static int ett_sabp_cbs_page_content = -1;

#include "packet-sabp-ett.c"

/* Global variables */
static guint32 ProcedureCode;
static guint32 ProtocolIE_ID;
static guint32 ProtocolExtensionID;
static guint8 sms_encoding;

#define SABP_PORT 3452

/* Dissector tables */
static dissector_table_t sabp_ies_dissector_table;
static dissector_table_t sabp_extension_dissector_table;
static dissector_table_t sabp_proc_imsg_dissector_table;
static dissector_table_t sabp_proc_sout_dissector_table;
static dissector_table_t sabp_proc_uout_dissector_table;

static dissector_handle_t sabp_handle;
static dissector_handle_t sabp_tcp_handle;

static int dissect_ProtocolIEFieldValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *);
static int dissect_ProtocolExtensionFieldExtensionValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *);
static int dissect_InitiatingMessageValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *);
static int dissect_SuccessfulOutcomeValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *);
static int dissect_UnsuccessfulOutcomeValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *);
static void dissect_sabp_cb_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree);

#include "packet-sabp-fn.c"

static int dissect_ProtocolIEFieldValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  return (dissector_try_uint(sabp_ies_dissector_table, ProtocolIE_ID, tvb, pinfo, tree)) ? tvb_captured_length(tvb) : 0;
}

static int dissect_ProtocolExtensionFieldExtensionValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  return (dissector_try_uint(sabp_extension_dissector_table, ProtocolExtensionID, tvb, pinfo, tree)) ? tvb_captured_length(tvb) : 0;
}

static int dissect_InitiatingMessageValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  return (dissector_try_uint(sabp_proc_imsg_dissector_table, ProcedureCode, tvb, pinfo, tree)) ? tvb_captured_length(tvb) : 0;
}

static int dissect_SuccessfulOutcomeValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  return (dissector_try_uint(sabp_proc_sout_dissector_table, ProcedureCode, tvb, pinfo, tree)) ? tvb_captured_length(tvb) : 0;
}

static int dissect_UnsuccessfulOutcomeValue(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
  return (dissector_try_uint(sabp_proc_uout_dissector_table, ProcedureCode, tvb, pinfo, tree)) ? tvb_captured_length(tvb) : 0;
}


/* 3GPP TS 23.041 version 11.4.0
 * 9.4.2.2.5 CB Data
 */
static void
dissect_sabp_cb_data(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
  proto_item *cbs_page_item;
  proto_tree *subtree;
  tvbuff_t *page_tvb, *unpacked_tvb;
  int offset = 0;
  int n;
  guint8 nr_pages, len, cb_inf_msg_len;


  /* Octet 1 Number-of-Pages */
  nr_pages = tvb_get_guint8(tvb, offset);
  proto_tree_add_item(tree, hf_sabp_no_of_pages, tvb, offset, 1, ENC_BIG_ENDIAN);
  offset++;
  /*
   * NOTE: n equal to or less than 15
   */
  if(nr_pages > 15){
    /* Error */
    return;
  }
  for (n = 0; n < nr_pages; n++) {
    subtree = proto_tree_add_subtree_format(tree, tvb, offset, 83, ett_sabp_cbs_page, NULL,
                "CB page %u data",  n+1);
    /* octet 2 - 83 CBS-Message-Information-Page 1  */
    cbs_page_item = proto_tree_add_item(subtree, hf_sabp_cb_msg_inf_page, tvb, offset, 82, ENC_NA);
    cb_inf_msg_len = tvb_get_guint8(tvb,offset+82);
    page_tvb = tvb_new_subset_length(tvb, offset, cb_inf_msg_len);
    unpacked_tvb = dissect_cbs_data(sms_encoding, page_tvb, subtree, pinfo, 0);
    len = tvb_captured_length(unpacked_tvb);
    if (unpacked_tvb != NULL){
      if (tree != NULL){
        proto_tree *cbs_page_subtree = proto_item_add_subtree(cbs_page_item, ett_sabp_cbs_page_content);
        proto_tree_add_item(cbs_page_subtree, hf_sabp_cbs_page_content, unpacked_tvb, 0, len, ENC_UTF_8|ENC_NA);
      }
    }

    offset = offset+82;
    /* 84 CBS-Message-Information-Length 1 */
    proto_tree_add_item(subtree, hf_sabp_cb_inf_len, tvb, offset, 1, ENC_BIG_ENDIAN);
    offset++;
  }
}

static int
dissect_sabp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
  proto_item  *sabp_item = NULL;
  proto_tree  *sabp_tree = NULL;

  /* make entry in the Protocol column on summary display */
  col_set_str(pinfo->cinfo, COL_PROTOCOL, PSNAME);

  /* create the sabp protocol tree */
  sabp_item = proto_tree_add_item(tree, proto_sabp, tvb, 0, -1, ENC_NA);
  sabp_tree = proto_item_add_subtree(sabp_item, ett_sabp);

  return dissect_SABP_PDU_PDU(tvb, pinfo, sabp_tree, NULL);
}

/* Note a little bit of a hack assumes length max takes two bytes and that the length starts at byte 4 */
static int
dissect_sabp_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data)
{
  guint32 type_length, msg_len;
  guint tvb_length;
  int bit_offset;
  gboolean is_fragmented;
  asn1_ctx_t asn1_ctx;
  asn1_ctx_init(&asn1_ctx, ASN1_ENC_PER, TRUE, pinfo);

  tvb_length = tvb_reported_length(tvb);

  if (tvb_length < 5) {
    pinfo->desegment_offset = 0;
    pinfo->desegment_len = DESEGMENT_ONE_MORE_SEGMENT;
    return tvb_captured_length(tvb);
  }

  /* Length should be in the 3:d octet */
  bit_offset = 24;
  /* Get the length of the sabp packet. Offset in bits */
  do {
    bit_offset = dissect_per_length_determinant(tvb, bit_offset, &asn1_ctx, NULL, -1, &type_length, &is_fragmented);
    bit_offset += 8*type_length;
    msg_len = (bit_offset + 7) >> 3;
    if (is_fragmented) {
      /* Next length field will take 1 or 2 bytes; let's ask for the maximum */
      msg_len += 2;
    }
    if (msg_len > tvb_length) {
      pinfo->desegment_offset = 0;
      pinfo->desegment_len = msg_len - tvb_length;
      return tvb_captured_length(tvb);
    }
  } while (is_fragmented);

  return dissect_sabp(tvb, pinfo, tree, data);
}

/*--- proto_register_sabp -------------------------------------------*/
void proto_register_sabp(void) {

  /* List of fields */

  static hf_register_info hf[] = {
    { &hf_sabp_no_of_pages,
      { "Number-of-Pages", "sabp.no_of_pages",
        FT_UINT8, BASE_DEC, NULL, 0,
        NULL, HFILL }},
    { &hf_sabp_cb_msg_inf_page,
      { "CBS-Message-Information-Page", "sabp.cb_msg_inf_page",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_sabp_cbs_page_content,
      { "CBS Page Content", "sabp.cb_page_content",
        FT_STRING, BASE_NONE, NULL, 0,
        NULL, HFILL }},
    { &hf_sabp_cb_inf_len,
      { "CBS-Message-Information-Length", "sabp.cb_inf_len",
        FT_UINT8, BASE_DEC, NULL, 0,
        NULL, HFILL }},

#include "packet-sabp-hfarr.c"
  };

  /* List of subtrees */
  static gint *ett[] = {
    &ett_sabp,
    &ett_sabp_e212,
    &ett_sabp_cbs_data_coding,
    &ett_sabp_bcast_msg,
    &ett_sabp_cbs_serial_number,
    &ett_sabp_cbs_new_serial_number,
    &ett_sabp_cbs_page,
    &ett_sabp_cbs_page_content,
#include "packet-sabp-ettarr.c"
  };


  /* Register protocol */
  proto_sabp = proto_register_protocol(PNAME, PSNAME, PFNAME);
  /* Register fields and subtrees */
  proto_register_field_array(proto_sabp, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));

  /* Register dissector */
  sabp_handle = register_dissector("sabp", dissect_sabp, proto_sabp);
  sabp_tcp_handle = register_dissector("sabp.tcp", dissect_sabp_tcp, proto_sabp);

  /* Register dissector tables */
  sabp_ies_dissector_table = register_dissector_table("sabp.ies", "SABP-PROTOCOL-IES", proto_sabp, FT_UINT32, BASE_DEC);
  sabp_extension_dissector_table = register_dissector_table("sabp.extension", "SABP-PROTOCOL-EXTENSION", proto_sabp, FT_UINT32, BASE_DEC);
  sabp_proc_imsg_dissector_table = register_dissector_table("sabp.proc.imsg", "SABP-ELEMENTARY-PROCEDURE InitiatingMessage", proto_sabp, FT_UINT32, BASE_DEC);
  sabp_proc_sout_dissector_table = register_dissector_table("sabp.proc.sout", "SABP-ELEMENTARY-PROCEDURE SuccessfulOutcome", proto_sabp, FT_UINT32, BASE_DEC);
  sabp_proc_uout_dissector_table = register_dissector_table("sabp.proc.uout", "SABP-ELEMENTARY-PROCEDURE UnsuccessfulOutcome", proto_sabp, FT_UINT32, BASE_DEC);
}


/*--- proto_reg_handoff_sabp ---------------------------------------*/
void
proto_reg_handoff_sabp(void)
{
  dissector_add_uint_with_preference("udp.port", SABP_PORT, sabp_handle);
  dissector_add_uint_with_preference("tcp.port", SABP_PORT, sabp_tcp_handle);
  dissector_add_uint("sctp.ppi", SABP_PAYLOAD_PROTOCOL_ID, sabp_handle);

#include "packet-sabp-dis-tab.c"
}


