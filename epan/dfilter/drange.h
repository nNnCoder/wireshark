/** @file
 *
 * Routines for providing general range support to the dfilter library
 *
 * Copyright (c) 2000 by Ed Warnicke <hagbard@physics.rutgers.edu>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs
 * Copyright 1999 Gerald Combs
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __DRANGE_H__
#define __DRANGE_H__

#include <glib.h>
#include "ws_symbol_export.h"
#include "ws_attributes.h"

/* Please don't directly manipulate these structs.  Please use
 * the methods provided.  If you REALLY can't do what you need to
 * do with the methods provided please write new methods that do
 * what you need, put them into the drange object here, and limit
 * your direct manipulation of the drange and drange_node structs to
 * here.
 */

typedef enum {
	DRANGE_NODE_END_T_UNINITIALIZED,
	DRANGE_NODE_END_T_LENGTH,
	DRANGE_NODE_END_T_OFFSET,
	DRANGE_NODE_END_T_TO_THE_END
} drange_node_end_t;

typedef struct _drange_node {
  gint			start_offset;
  gint			length;
  gint 			end_offset;
  drange_node_end_t	ending;
} drange_node;

typedef struct _drange {
  GSList* range_list;
  gboolean has_total_length;
  gint total_length;
  gint min_start_offset;
  gint max_start_offset;
} drange_t;

/* drange_node constructor */
drange_node* drange_node_new(void);

/* drange_node constructor */
drange_node* drange_node_from_str(const char *range_str, char **err_ptr);

/* drange_node destructor */
void drange_node_free(drange_node* drnode);

/* Call drange_node destructor on all list items */
void drange_node_free_list(GSList* list);

/* drange_node accessors */
gint drange_node_get_start_offset(drange_node* drnode);
gint drange_node_get_length(drange_node* drnode);
gint drange_node_get_end_offset(drange_node* drnode);
drange_node_end_t drange_node_get_ending(drange_node* drnode);

/* drange_node mutators */
void drange_node_set_start_offset(drange_node* drnode, gint offset);
void drange_node_set_length(drange_node* drnode, gint length);
void drange_node_set_end_offset(drange_node* drnode, gint offset);
void drange_node_set_to_the_end(drange_node* drnode);

/* drange constructor */
drange_t * drange_new(void);
drange_t * drange_new_from_list(GSList *list);
drange_t * drange_dup(drange_t *org);

/* drange destructor, only use this if you used drange_new() to creat
 * the drange
 */
void drange_free(drange_t* dr);

/* drange accessors */
gboolean drange_has_total_length(drange_t* dr);
gint drange_get_total_length(drange_t* dr);
gint drange_get_min_start_offset(drange_t* dr);
gint drange_get_max_start_offset(drange_t* dr);

/* drange mutators */
void drange_append_drange_node(drange_t* dr, drange_node* drnode);
void drange_prepend_drange_node(drange_t* dr, drange_node* drnode);
void drange_foreach_drange_node(drange_t* dr, GFunc func, gpointer funcdata);

char *drange_tostr(drange_t *dr);

#endif /* ! __DRANGE_H__ */
