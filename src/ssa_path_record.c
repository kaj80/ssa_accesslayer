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

#include <string.h>
#include <math.h>
#include <ssa_comparison.h>

typedef enum _ssa_pr_status_t {
	SSA_PR_SUCCESS,
	SSA_PR_ERROR
} ssa_pr_status_t;

typedef struct ssa_path_parms {
	ib_net16_t pkey;
	uint8_t mtu;
	uint8_t rate;
	uint8_t sl;
	uint8_t pkt_life;
	boolean_t reversible;
} ssa_path_parms_t;


static struct ep_guid_to_lid_tbl_rec* find_guid_to_lid_rec(struct ssa_db_diff* p_ssa_db_diff,
		const be64_t port_guid)
{
	size_t i =0;
	struct db_dataset *p_dataset = p_ssa_db_diff->db_tables[SSA_TABLE_ID_GUID_TO_LID];
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	const size_t n = ntohll(p_dataset->set_count);

	for (i = 0; i < n; i++) {
		if (port_guid == p_guid_to_lid_tbl[i].guid) 
			return p_guid_to_lid_tbl+i;
	}
	return NULL;
}

static ssa_pr_status_t ssa_pr_half_world(struct ssa_db_diff* p_ssa_db_diff, 
		be64_t port_guid)
{
	ep_guid_to_lid_tbl_rec *p_source_rec = NULL;
	const size_t guid_to_lid_count = ntohll(p_ssa_db_diff->db_tables[SSA_TABLE_ID_GUID_TO_LID].set_count);
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl =
		        (struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	size_t i = 0;
	unsigned int source_lids_count = 0;
	uint16_t source_base_lid = 0;

	p_source_rec = find_guid_to_lid_rec(p_ssa_db_diff,port_guid);

	if (NULL == p_guid_to_lid_rec) {
		return SSA_PR_ERROR;
	}

	source_lids_count = pow(2,p_source_rec->lmc);
	source_base_lid = ntohs(p_source_rec->lid);

	for (i = 0; i < guid_to_lid_count; i++) {
		/* code */
		unsigned int dest_lids_count = 0;
		uint16_t dest_base_lid = 0;
		uint16_t source_lid = 0, dest_lid = 0;


		ep_guid_to_lid_tbl_rec* p_dest_rec = p_guid_to_lid_tbl[i];
		dest_lids_count = pow(2, p_dest_rec->lmc);
		dest_base_lid = ntohs(p_dest_rec->lid);

		for(source_lid = source_base_lid ; source_lid<=(source_base_lid+source_lids_count-1); ++source_lid){
			for(dest_lid = dest_base_lid ; dest_lid<=(dest_base_lid+dest_lids_count-1); ++dest_lid){
				ssa_path_parms path_prm;
				if(SSA_PR_SUCCESS == ssa_pr_path_params(p_ssa_db_diff,p_source_rec,p_dest_rec,&path_prm)){
					ssa_path_parms revers_path_prm;
					ssa_pr_status_t revers_path_res = ssa_pr_path_params(p_ssa_db_diff,p_dest_rec,p_source_rec,&revers_path_prm) ;

					path_prm.reversible = SSA_PR_SUCCESS == revers_path_res ;
					ssa_pr_path_output(p_source_rec,p_dest_rec,&path_prm);
				}
			}
		}
	}
}
										
static uint8_t find_destination_port(const struct ssa_db_diff* p_ssa_db_diff,
		const uint16_t dest_lid)
{
	size_t i =0;
	struct db_dataset *p_lft_top_dataset = p_ssa_db_diff->db_tables[SSA_TABLE_ID_LFT_TOP];
	struct ep_lft_top_tbl_rec *p_lft_top_tbl = 
		(struct ep_lft_top_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_LFT_TOP];
	const size_t n = ntohll(p_dataset->set_count);

	
}

static ssa_pr_status_t ssa_pr_path_params(const struct ssa_db_diff* p_ssa_db_diff,
		const ep_guid_to_lid_tbl_rec *p_source_rec,
		const ep_guid_to_lid_tbl_rec *p_dest_rec,
		ssa_path_parms* p_path_prm)
{
	
}

static void ssa_pr_path_output(
		const ep_guid_to_lid_tbl_rec *p_source_rec,
		const ep_guid_to_lid_tbl_rec *p_dest_rec,
		const ssa_path_parms *p_path_prm)
{
	printf("source guid: 0x%" PRIx64 " source lid: %"SCNu16" dest guid: 0x%" PRIx64 " dest lid: %"SCNu16" mtu: %u rate: %u reversible: %u\n",
			ntohll(p_source_rec->guid),ntohs(p_source_rec->lid),ntohll(p_dest_rec->guid),ntohs(p_dest_rec->lid),
			p_path_prm->mtu,p_path_prm->rate,p_path_prm->reversible);

}
