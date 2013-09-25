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

#include <glib.h>

/*
 * Internal API for data
 */
struct ssa_pr_smdb_index {
	uint8_t *is_switch_lookup;
	uint16_t *lft_top_lookup;
	GHashTable *lft_block_hash;
	GHashTable *port_hash;
	GHashTable *link_hash;
	uint64_t epoch;
};

extern int ssa_pr_build_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb);
extern void ssa_pr_destroy_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index);
extern int ssa_pr_rebuild_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb);
#endif /* end of include guard: SSA_PATH_RECORD_DATA_H */
