/*
 * Copyright 2004-2013 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ifndef SSA_PATH_RECORD_DATA_H
#define SSA_PATH_RECORD_DATA_H

/*
 * Internal API for data
 */

#include <glib.h>

#define LFT_NO_PATH 255
#define MAX_LOOKUP_LID 0xBFFF
#define MAX_LOOKUP_PORT 254
#define MAX_LFT_BLOCK_MUM (MAX_LOOKUP_LID/64)

struct ssa_pr_smdb_index {
	uint8_t is_switch_lookup[MAX_LOOKUP_LID];
	uint16_t lft_top_lookup[MAX_LOOKUP_LID];
	uint64_t* lft_block_lookup[MAX_LOOKUP_LID];
	uint64_t ca_port_lookup[MAX_LOOKUP_LID];
	uint64_t* switch_port_lookup[MAX_LOOKUP_LID];
	uint64_t ca_link_lookup[MAX_LOOKUP_LID];
	uint64_t* switch_link_lookup[MAX_LOOKUP_LID];
	uint64_t epoch;
};

extern int ssa_pr_build_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb);

extern void ssa_pr_destroy_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index);

extern int ssa_pr_rebuild_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb);

extern const struct ep_guid_to_lid_tbl_rec 
*find_guid_to_lid_rec_by_guid(const struct ssa_db_smdb *p_ssa_db_smdb,
		const be64_t port_guid);

extern const struct ep_port_tbl_rec *find_port(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t lid,
		const int port_num);

extern int find_destination_port(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t source_lid,
		const be16_t dest_lid);

extern const struct ep_link_tbl_rec *find_link(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t lid,
		const int port_num);
#endif /* end of include guard: SSA_PATH_RECORD_DATA_H */
