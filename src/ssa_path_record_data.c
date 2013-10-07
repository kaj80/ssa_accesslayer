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
#include <math.h>
#include <ssa_smdb.h>
#include "ssa_path_record_helper.h"
#include "ssa_path_record_data.h"


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

	p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	SSA_ASSERT(p_guid_to_lid_tbl);

	memset(p_ssa_pr_smdb_index->is_switch_lookup,'\0',MAX_LOOKUP_LID);

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

	p_lft_top_tbl = 
		(struct ep_lft_top_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LFT_TOP];
	SSA_ASSERT(p_lft_top_tbl );

	memset(p_ssa_pr_smdb_index->lft_top_lookup,'\0',MAX_LOOKUP_LID*sizeof(uint16_t));

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

static int build_lft_block_lookup(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index,
		const struct ssa_db_smdb *p_ssa_db_smdb)
{
	size_t i = 0, count = 0;
	const struct ep_lft_block_tbl_rec *p_lft_block_tbl = NULL;
	size_t lookup_size = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_ssa_pr_smdb_index);

	p_lft_block_tbl =(struct ep_lft_block_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LFT_BLOCK];
	SSA_ASSERT(p_lft_block_tbl);

	memset(p_ssa_pr_smdb_index->lft_block_lookup,'\0',MAX_LOOKUP_LID * sizeof(uint64_t*));

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LFT_BLOCK);

	for (i = 0; i < count; i++) {
		size_t j = 0;
		uint64_t *block_lookup = 
			p_ssa_pr_smdb_index->lft_block_lookup[ntohs(p_lft_block_tbl[i].lid)];
		if(!block_lookup) {
			block_lookup = (uint64_t*)malloc(MAX_LFT_BLOCK_MUM * sizeof(uint64_t));
			lookup_size += MAX_LFT_BLOCK_MUM * sizeof(uint64_t);

			p_ssa_pr_smdb_index->lft_block_lookup[ntohs(p_lft_block_tbl[i].lid)] =
				block_lookup;

			for(j = 0; j < MAX_LFT_BLOCK_MUM; ++j)
				block_lookup[j] = count + 1;
		}
		block_lookup[ntohs(p_lft_block_tbl[i].block_num)] = i;
	}

	SSA_PR_LOG_INFO("LFT lookup size: %u bytes",lookup_size);
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
	if(res) {
		SSA_PR_LOG_ERROR("Build for is_switch_lookup is failed");
		return res;
	}
	res = build_lft_top_lookup(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(res) {
		SSA_PR_LOG_ERROR("Build for lft_top is failed");
		return res;
	}
	res = build_port_index(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(res) {
		SSA_PR_LOG_ERROR("Build for port index is failed");
		return res;
	}
	res = build_lft_block_lookup(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(res) {
		SSA_PR_LOG_ERROR("Build for lft block lookup is failed");
		return res;
	}
	res = build_link_index(p_ssa_pr_smdb_index,p_ssa_db_smdb);
	if(res) {
		SSA_PR_LOG_ERROR("Build for link index is failed");
		return res;
	}

	return 0;
}


void ssa_pr_destroy_indexes(struct ssa_pr_smdb_index *p_ssa_pr_smdb_index)
{
	size_t i = 0;

	SSA_ASSERT(p_ssa_pr_smdb_index);

	memset(p_ssa_pr_smdb_index->is_switch_lookup,'\0',MAX_LOOKUP_LID);

	memset(p_ssa_pr_smdb_index->lft_top_lookup ,'\0',MAX_LOOKUP_LID * sizeof(uint16_t));

	if(p_ssa_pr_smdb_index->port_hash) {
		g_hash_table_destroy(p_ssa_pr_smdb_index->port_hash);
		p_ssa_pr_smdb_index->port_hash = NULL;
	}

	if(p_ssa_pr_smdb_index->link_hash) {
		g_hash_table_destroy(p_ssa_pr_smdb_index->link_hash);
		p_ssa_pr_smdb_index->link_hash = NULL;
	}
	
	for(i = 0; i < MAX_LOOKUP_LID; ++i) {
		if(p_ssa_pr_smdb_index->lft_block_lookup[i])
			free(p_ssa_pr_smdb_index->lft_block_lookup[i]);
	}
	memset(p_ssa_pr_smdb_index->lft_block_lookup,'\0',MAX_LOOKUP_LID * sizeof(uint16_t*));

	p_ssa_pr_smdb_index->epoch = -1;
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
	uint64_t smdb_epoch = 0;
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
		if(res) {
			SSA_PR_LOG_ERROR("SMDB index creation is failed. epoch : %"PRIu64,smdb_epoch);
			return res;
		}
		p_ssa_pr_smdb_index->epoch = smdb_epoch;
		SSA_PR_LOG_INFO("SMDB index was created. epoch : %"PRIu64,p_ssa_pr_smdb_index->epoch);
	}
	return 0;
}

const struct ep_guid_to_lid_tbl_rec *find_guid_to_lid_rec_by_guid(const struct ssa_db_smdb *p_ssa_db_smdb,
		const be64_t port_guid)
{
	size_t i = 0;
	const struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = NULL;
	size_t count = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(port_guid);

	p_guid_to_lid_tbl = (struct ep_guid_to_lid_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	SSA_ASSERT(p_guid_to_lid_tbl);

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_GUID_TO_LID);

	for (i = 0; i < count; i++) {
		if (port_guid == p_guid_to_lid_tbl[i].guid) 
			return p_guid_to_lid_tbl + i;
	}

	SSA_PR_LOG_ERROR("GUID to LID record is not found. GUID: 0x%016"PRIx64,ntohll(port_guid));

	return NULL;
}

int find_destination_port(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t source_lid,
		const be16_t dest_lid)
{
	size_t i = 0;

	struct ep_lft_block_tbl_rec *p_lft_block_tbl = NULL;
	size_t lft_block_count = 0;

	size_t lft_block_num = 0;
	size_t lft_port_shift = 0;
	size_t lft_block_index = 0;

	gpointer value = NULL;
	gpointer org_key = NULL;

	uint16_t lft_top = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_index);
	SSA_ASSERT(source_lid);
	SSA_ASSERT(dest_lid);

	p_lft_block_tbl =(struct ep_lft_block_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LFT_BLOCK];
	SSA_ASSERT(p_lft_block_tbl);

	lft_block_count  = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LFT_BLOCK);

	/*
	 * Optimisation. If IB_SMP_DATA_SIZE is power of 2 we can use shift istead of division
	 *
	 *  lft_block_num = floorl(ntohs(dest_lid) / IB_SMP_DATA_SIZE);
	 */
	lft_block_num = ntohs(dest_lid) >> 6;
	lft_port_shift = ntohs(dest_lid) % IB_SMP_DATA_SIZE;
	lft_top = p_index->lft_top_lookup[ntohs(source_lid)];

	if(ntohs(dest_lid) > lft_top) {
		SSA_PR_LOG_ERROR("LFT routing is failed. Destination LID exceeds LFT top . "
				"Source LID (0x%"SCNx16") Destination LID: (0x%"SCNx16") LFT top: %u",
			ntohs(source_lid),ntohs(dest_lid),lft_top);
		return -1;
	}

	if (!p_index->lft_block_lookup[ntohs(source_lid)] || 
			p_index->lft_block_lookup[ntohs(source_lid)][lft_block_num] > lft_block_count) {
		SSA_PR_LOG_ERROR("LFT routing is failed. Destination LID exceeds LFT top . "
				"Source LID (0x%"SCNx16") Destination LID: (0x%"SCNx16") LFT top: %u",
			ntohs(source_lid),ntohs(dest_lid),lft_top);
		return -1;
	}

	lft_block_index = p_index->lft_block_lookup[ntohs(source_lid)][lft_block_num];
	return p_lft_block_tbl[lft_block_index].block[lft_port_shift];
}

const struct ep_port_tbl_rec *find_port(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t lid,
		const int port_num)
{
	size_t i = 0;
	const struct ep_port_tbl_rec  *p_port_tbl = NULL;
	size_t count = 0;
	gpointer value = NULL;
	gpointer org_key = NULL;
	gboolean res = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_index);
	SSA_ASSERT(p_index->port_hash);
	SSA_ASSERT(p_index->is_switch_lookup);
	SSA_ASSERT(lid);

	p_port_tbl = (const struct ep_port_tbl_rec*)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_PORT];
	SSA_ASSERT(p_port_tbl );

	count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_PORT);

	res = g_hash_table_lookup_extended(p_index->port_hash,
			be16uint8_key(lid,p_index->is_switch_lookup[ntohs(lid)]?
				port_num:NO_REAL_PORT_NUM),&org_key,&value);

	if(!res || GPOINTER_TO_INT(value) >= count) {
		if(port_num >= 0) {
			SSA_PR_LOG_ERROR("Port is not found. LID: 0x%"SCNx16" Port num: %d",
					ntohs(lid),port_num);
		} else {
			SSA_PR_LOG_ERROR("Port is not found. LID: 0x%"SCNx16,ntohs(lid));
		}
	}

	return p_port_tbl + GPOINTER_TO_INT(value);
}

const struct ep_link_tbl_rec *find_link(const struct ssa_db_smdb *p_ssa_db_smdb,
		const struct ssa_pr_smdb_index *p_index,
		const be16_t lid,
		const int port_num)
{
	size_t i = 0;
	const struct ep_link_tbl_rec *p_link_tbl =  NULL;
	size_t link_count = 0;
	gpointer value = NULL;
	gpointer org_key = NULL;
	gboolean res = 0;

	SSA_ASSERT(p_ssa_db_smdb);
	SSA_ASSERT(p_index);
	SSA_ASSERT(p_index->port_hash);
	SSA_ASSERT(p_index->is_switch_lookup);
	SSA_ASSERT(lid);

	p_link_tbl = (const struct ep_link_tbl_rec*)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_LINK];
	SSA_ASSERT(p_link_tbl);

	res = g_hash_table_lookup_extended(p_index->link_hash,
			be16uint8_key(lid,p_index->is_switch_lookup[ntohs(lid)] ?
				port_num : NO_REAL_PORT_NUM),&org_key,&value);

	link_count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_LINK);

	if(!res || GPOINTER_TO_INT(value) >= link_count) {
		if(port_num >= 0) {
			SSA_PR_LOG_ERROR("Link is not found. LID: 0x%"SCNx16" Port num: %u",
					ntohs(lid),port_num);
		} else {
			SSA_PR_LOG_ERROR("Link is not found. LID: 0x%"SCNx16,ntohs(lid));
		}
	}

	return p_link_tbl + GPOINTER_TO_INT(value);
}
