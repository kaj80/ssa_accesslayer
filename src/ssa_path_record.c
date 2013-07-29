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

#define MIN(X,Y) ((X) < (Y) ? : (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? : (X) : (Y))

#define MAX_HOPS 64

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


static size_t get_dataset_count(struct ssa_db_diff* p_ssa_db_diff,
		unsigned int table_id)
{
	struct db_dataset *p_dataset = p_ssa_db_diff->db_tables[table_id];
	return ntohll(p_dataset->set_count);
}


static struct ep_guid_to_lid_tbl_rec* find_guid_to_lid_rec(struct ssa_db_diff* p_ssa_db_diff,
		const be64_t port_guid)
{
	size_t i =0;
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	const size_t n = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_GUID_TO_LID);

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
	const size_t guid_to_lid_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_GUID_TO_LID);
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
										
static int find_destination_port(const struct ssa_db_diff* p_ssa_db_diff,
		const uint16_t source_lid,
		const uint16_t dest_lid)
{
	size_t i =0;

	struct ep_lft_top_tbl_rec *p_lft_top_tbl = 
		(struct ep_lft_top_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_LFT_TOP];
	const size_t lft_top_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_LFT_TOP);

	struct ep_lft_block_tbl_rec *p_lft_block_tbl = 
		(struct ep_lft_block_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_LFT_BLOCK];
	const size_t lft_block_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_LFT_BLOCK);

	const size_t lft_block_num = floorl(dest_lid/IB_SMP_DATA_SIZE);
	const size_t lft_port_num = dest_lid%IB_SMP_DATA_SIZE;

	for (i = 0; i < lft_top_count && source_lid !=ntohs(p_lft_top_tbl[i].lid) ; i++);
	if(i >= lft_top_count || dest_lid > p_lft_top_tbl[i].lft_top){
		/*
		 * Report error
		 */
		return -1;
	}

	for (i = 0; i < lft_block_count && source_lid != ntohs(p_lft_block_tbl[i].lid) && l
			ft_block_num != ntohs(p_lft_block_tbl[i].block_num); i++);

	return i < lft_block_count ? p_lft_block_tbl[i].block[lft_port_num] : -1;
}

static ep_port_tbl_rec* find_port(const struct ssa_db_diff* p_ssa_db_diff,
		const uint16_t source_lid,
		const int port_num)
{

}

static ep_link_tbl_rec* find_link(const struct ssa_db_diff* p_ssa_db_diff,
		const uint16_t lid,
		const int port_num)
{
	size_t i = 0;
	struct ep_link_tbl_rec  *p_link_tbl = 
		(struct *ep_link_tbl_rec)p_ssa_db_diff->p_tables[SSA_TABLE_ID_LINK];
	const size_t link_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_LINK);

	for (i = 0; i < link_count && lid!=ntohs(p_link_tbl[i].from_lid) && port_num != p_link_tbl[i].from_port_num; i++);
	return i < link_count ? p_link_tbl[i] : NULL;
}

static ssa_pr_status_t ssa_pr_path_params(const struct ssa_db_diff* p_ssa_db_diff,
		const ep_guid_to_lid_tbl_rec *p_source_rec,
		const ep_guid_to_lid_tbl_rec *p_dest_rec,
		ssa_path_parms* p_path_prm)
{
	int source_port_num = -1 ; 
	int dest_port_num = -1 ;
	ep_port_tbl_rec *source_port = NULL ;
	ep_port_tbl_rec *dest_port = NULL ;
	ep_port_tbl_rec *port = NULL ;
	uint16_t source_lid = ntohs(p_source_rec->lid);
	uint16_t dest_lid = ntohs(p_dest_rec->lid);

	p_path_prm->mtu = p_path_prm->rate = p_path_prm->pkt_life = p_path_prm->hops = 0;

	if(p_source_rec->is_switch){
		source_port_num = find_destination_port(p_ssa_db_diff,source_lid,dest_lid);
		if(source_port_num < 0){
			fprintf(srferr,"Error: Destination port is not found. Switch lid:%"SCNu16" , Destination lid:%"SCNu16"\n",
					source_lid,dest_lid);
			return SSA_PR_ERROR;
		}
	}

	dest_port_num = p_dest_rec->is_switch ? 0 : -1 ;

	source_port = find_port(p_ssa_db_diff,source_lid,source_port_num);
	dest_port = find_port(p_ssa_db_diff,dest_lid,dest_port_num);

	p_path_prm->pkt_life = source_port == dest_port ? 0 : p_ssa_db_diff->subnet_timeout;
	p_path_prm->mtu = source_port->mtu;
	p_path_prm->rate = source_port->rate;

	port = source_port;
	while( port != dest_port){
		const ep_link_tbl_rec* link = find_link(p_ssa_db_diff,ntohs(port->port_lid),port->port_num);
		const uint16_t lid = ntohs(link->to_lid);
		const uint8_t port_num = link->to_port_num;

		if(NULL == link)
			return SSA_PR_ERROR;

		port = find_port(p_ssa_db_diff,lid,port_num);
		if(port == dest_port)
			break;

		p_path_prm->mtu = MIN(p_path_prm->mtu,port->neighbor_mtu);
		p_path_prm->rate = MIN(p_path_prm->rate,port->rate);

		port_num = find_destination_port(p_ssa_db_diff,lid,dest_lid);
		port = find_port(p_ssa_db_diff,lid,port_num);

		p_path_prm->mtu = MIN(p_path_prm->mtu,port->neighbor_mtu);
		p_path_prm->rate = MIN(p_path_prm->rate,port->rate);
		p_path_prm->hops++;

		if (p_path_prm->hops > MAX_HOPS)
			return SSA_PR_ERROR;
	}

	return SSA_PR_ERROR;
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
