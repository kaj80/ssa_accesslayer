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

#if HAVE_CONFIG_H
#  include <config.h>
#endif              /* HAVE_CONFIG_H */

#include <errno.h>
#include <glib.h>
#include <ssa_smdb.h>
#include "ssa_path_record_helper.h"
#include <ssa_path_record_data.h>


#define NO_REAL_PORT_NUM -1

inline static size_t get_dataset_count(const struct ssa_db_smdb *p_ssa_db_smdb,
		unsigned int table_id)
{
	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(table_id < SSA_TABLE_ID_MAX);
	SSA_ASSERT(&p_ssa_db_smdb->db_tables[table_id]);

	return ntohll(p_ssa_db_smdb->db_tables[table_id].set_count);
}

inline static gpointer be16uint8_key(const be16_t lid, uint8_t num)
{
	return GUINT_TO_POINTER((ntohs(lid) << 8) | num);
}

inline static gpointer be16b16_key(const be16_t lid, be16_t num)
{
	return GUINT_TO_POINTER((ntohs(lid) << 16) | ntohs(num));
}

static int build_is_switch_lookup(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	const struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = NULL;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);
	SSA_ASSERT(!p_ssa_pr_smdb_index->is_switch_lookup);

	p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	SSA_ASSERT(p_guid_to_lid_tbl);

	p_ssa_pr_smdb_index->is_switch_lookup = (uint8_t*)malloc(0xFFFF);
	if(!p_ssa_pr_smdb_index->is_switch_lookup) {
		int errsv = errno;
		SSA_PR_LOG_ERROR("Memory allocation error %d ",errsv);
		return -1;
	}
	memset(p_ssa_pr_smdb_index->is_switch_lookup,'\0',0xFFFF);

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_GUID_TO_LID);

	for (i = 0; i < count; i++)
		p_ssa_pr_smdb_index->is_switch_lookup[ntohs(p_guid_to_lid_tbl[i].lid)] =
		   	p_guid_to_lid_tbl[i].is_switch;

	return 0;
}


static int build_lft_top_lookup(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	struct ep_lft_top_tbl_rec *p_lft_top_tbl = NULL;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);
	SSA_ASSERT(!p_ssa_pr_smdb_index->lft_top_lookup);

	p_lft_top_tbl = 
		(struct ep_lft_top_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LFT_TOP];
	SSA_ASSERT(p_lft_top_tbl );

	p_ssa_pr_smdb_index->lft_top_lookup = (uint16_t*)malloc(0xFFFF*sizeof(uint16_t));
	if(!p_ssa_pr_smdb_index->lft_top_lookup) {
		int errsv = errno;
		SSA_PR_LOG_ERROR("Memory allocation error %d ",errsv);
		return -1;
	}
	memset(p_ssa_pr_smdb_index->lft_top_lookup,'\0',0xFFFF*sizeof(uint16_t));

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LFT_TOP);

	for (i = 0; i < count; i++)
		p_ssa_pr_smdb_index->lft_top_lookup[ntohs(p_lft_top_tbl[i].lid)] = ntohs(p_lft_top_tbl[i].lft_top);

	return 0;
}

static int build_port_index(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	const struct ep_port_tbl_rec  *p_port_tbl = NULL;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);
	SSA_ASSERT(!p_ssa_pr_smdb_index->port_hash);

	p_port_tbl = 
		(struct ep_port_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_PORT];
	SSA_ASSERT(p_port_tbl);

	p_ssa_pr_smdb_index->port_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	if(!p_ssa_pr_smdb_index->port_hash) {
		int errsv = errno;
		SSA_PR_LOG_ERROR("GLib hash table creation is failed");
		return -1;
	}

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_PORT);

	for (i = 0; i < count; i++) {
		g_hash_table_insert(p_ssa_pr_smdb_index->port_hash,
				be16uint8_key(p_port_tbl[i].port_lid,
					!(p_port_tbl[i].rate & SSA_DB_PORT_IS_SWITCH_MASK)?
				   	NO_REAL_PORT_NUM : p_port_tbl[i].port_num),
			   	GINT_TO_POINTER(i));
	}

	return 0;
}

static int build_lft_block_index(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	const struct ep_lft_block_tbl_rec *p_lft_block_tbl = NULL;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);
	SSA_ASSERT(!p_ssa_pr_smdb_index->lft_block_hash);

	p_lft_block_tbl =(struct ep_lft_block_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LFT_BLOCK];
	SSA_ASSERT(p_lft_block_tbl);

	p_ssa_pr_smdb_index->lft_block_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	if(!p_ssa_pr_smdb_index->lft_block_hash) {
		int errsv = errno;
		SSA_PR_LOG_ERROR("GLib hash table creation is failed");
		return -1;
	}

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LFT_BLOCK);

	for (i = 0; i < count; i++) {
		g_hash_table_insert(p_ssa_pr_smdb_index->lft_block_hash,
				be16b16_key(p_lft_block_tbl[i].lid,
					p_lft_block_tbl[i].block_num),
					GINT_TO_POINTER(i));
	}

	return 0;
}
static int build_link_index(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	const struct ep_link_tbl_rec  *p_link_tbl =  NULL;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);
	SSA_ASSERT(p_ssa_pr_smdb_index->is_switch_lookup);
	SSA_ASSERT(!p_ssa_pr_smdb_index->link_hash);

	p_link_tbl = (const struct ep_link_tbl_rec*)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LINK];
	SSA_ASSERT(p_link_tbl);

	p_ssa_pr_smdb_index->link_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	if(!p_ssa_pr_smdb_index->link_hash) {
		int errsv = errno;
		SSA_PR_LOG_ERROR("GLib hash table creation is failed");
		return -1;
	}

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LINK);

	for (i = 0; i < count; i++) {
		g_hash_table_insert(p_ssa_pr_smdb_index->link_hash,
				be16uint8_key(p_link_tbl[i].from_lid,
					p_ssa_pr_smdb_index->is_switch_lookup[ntohs(p_link_tbl[i].from_lid)]?
					p_link_tbl[i].from_port_num:NO_REAL_PORT_NUM),
				GINT_TO_POINTER(i));
	}

	return 0;
}

int ssa_pr_build_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	int res = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);

	res = build_is_switch_lookup(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(!res) {
		SSA_PR_LOG_ERROR("Build for is_switch_lookup is failed");
		return res;
	}
	res = build_lft_top_lookup(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(!res) {
		SSA_PR_LOG_ERROR("Build for lft_top is failed");
		return res;
	}
	res = build_port_index(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(!res) {
		SSA_PR_LOG_ERROR("Build for port index is failed");
		return res;
	}
	res = build_lft_block_index(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(!res) {
		SSA_PR_LOG_ERROR("Build for lft block index is failed");
		return res;
	}
	res = build_link_index(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(!res) {
		SSA_PR_LOG_ERROR("Build for link index is failed");
		return res;
	}

	return 0;
}


void ssa_pr_destroy_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index)
{
	SSA_ASSERT(p_ssa_pr_smdb_index);

	if(p_ssa_pr_smdb_index->is_switch_lookup) {
		free(p_ssa_pr_smdb_index->is_switch_lookup);
		p_ssa_pr_smdb_index->is_switch_lookup = NULL;
	}

	if(p_ssa_pr_smdb_index->lft_top_lookup) {
		free(p_ssa_pr_smdb_index->lft_top_lookup);
		p_ssa_pr_smdb_index->lft_top_lookup = NULL;
	}
	
	if(p_ssa_pr_smdb_index->port_hash) {
		g_hash_table_destroy(p_ssa_pr_smdb_index->port_hash);
		p_ssa_pr_smdb_index->port_hash = NULL;
	}

	if(p_ssa_pr_smdb_index->link_hash) {
		g_hash_table_destroy(p_ssa_pr_smdb_index->link_hash);
		p_ssa_pr_smdb_index->link_hash = NULL;
	}

	if(p_ssa_pr_smdb_index->lft_block_hash) {
		g_hash_table_destroy(p_ssa_pr_smdb_index->lft_block_hash);
		p_ssa_pr_smdb_index->lft_block_hash = NULL;
	}
}

static int epoch_table_ids[] = {
	SSA_TABLE_ID_GUID_TO_LID,
	SSA_TABLE_ID_LINK,
	SSA_TABLE_ID_PORT,
	SSA_TABLE_ID_LFT_TOP,
	SSA_TABLE_ID_LFT_BLOCK
};

int ssa_pr_rebuild_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	int i = 0;
	uint64_t smdb_epoch = -1;
	int res = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);

	for(i = 0; i < sizeof(epoch_table_ids)/sizeof(epoch_table_ids[0]); ++i) {
		const struct db_dataset *p_dataset = &p_ssa_db_smdb->db_tables[epoch_table_ids[i]];
		smdb_epoch = smdb_epoch > ntohll(p_dataset->epoch) ? smdb_epoch : ntohll(p_dataset->epoch);
	}

	if(p_ssa_pr_smdb_index->epoch != smdb_epoch) {
		ssa_pr_destroy_indexes(p_ssa_pr_smdb_index);
		res = ssa_pr_build_indexes(p_ssa_pr_smdb_index,p_ssa_db_smdb);
		if(!res) {
			SSA_PR_LOG_ERROR("SMDB index creation is failed. epoch : %ll",smdb_epoch);
			return res;
		}
		p_ssa_pr_smdb_index->epoch = smdb_epoch;
		SSA_PR_LOG_INFO("SMDB index was created. epoch : %ll",p_ssa_pr_smdb_index->epoch);
	}
	return 0;
}
