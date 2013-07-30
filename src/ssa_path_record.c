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
#include <ssa_path_record.h>

#define MIN(X,Y) ((X) < (Y) ? : (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? : (X) : (Y))

#define MAX_HOPS 64


static size_t get_dataset_count(struct ssa_db_diff* p_ssa_db_diff,
		unsigned int table_id)
{
	struct db_dataset *p_dataset = p_ssa_db_diff->db_tables[table_id];
	return ntohll(p_dataset->set_count);
}
/*
typedef int record_cmp(const void*,const void*);
static void* find_record(const struct ssa_db_diff* p_ssa_db_diff,
		const unsigned int table_id,
		const size_t record_size,
		record_cmp cmp,
		const void* prm)
{
	size_t i =0 ;
	const size_t count = get_dataset_count(p_ssa_db_diff,table_id);

	for (i = 0; i < count; i++) {
		if(cmp(p_ssa_db_diff->p_tables[table_id]+count*record_size,prm))
			return p_ssa_db_diff->p_tables[table_id]+count*record_size;
	}
	return NULL;
}

static int node_guid_cmp(const void* record, const void* prm)
{
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(ep_guid_to_lid_tbl_rec*)record;
	return ntohs(record->guid) == *(uint64_t*)prm; 
}
*/

static struct ep_guid_to_lid_tbl_rec* find_guid_to_lid_rec_by_guid(const struct ssa_db_diff* p_ssa_db_diff,
		const be64_t port_guid)
{
	size_t i =0;
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	const size_t count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_GUID_TO_LID);

	for (i = 0; i < count; i++) {
		if (port_guid == p_guid_to_lid_tbl[i].guid) 
			return p_guid_to_lid_tbl+i;
	}
	return NULL;
}

static struct ep_guid_to_lid_tbl_rec* find_guid_to_lid_rec_by_lid(const struct ssa_db_diff* p_ssa_db_diff,
		const be16_t base_lid)
{
	size_t i =0;
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	const size_t count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_GUID_TO_LID);

	for (i = 0; i < count; i++) {
		if (base_lid == p_guid_to_lid_tbl[i].lid) 
			return p_guid_to_lid_tbl+i;
	}
	return NULL;
}

ssa_pr_status_t ssa_pr_half_world(struct ssa_db_diff* p_ssa_db_diff, 
		be64_t port_guid,
		ssa_pr_path_dump dump_clbk);
{
	ep_guid_to_lid_tbl_rec *p_source_rec = NULL;
	const size_t guid_to_lid_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_GUID_TO_LID);
	struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl =
		        (struct ep_guid_to_lid_tbl_rec *)p_ssa_db_diff->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	size_t i = 0;
	unsigned int source_lids_count = 0;
	uint16_t source_base_lid = 0;
	uint16_t source_last_lid = 0;
	

	p_source_rec = find_guid_to_lid_rec_by_guid(p_ssa_db_diff,port_guid);

	if (NULL == p_guid_to_lid_rec) {
		return SSA_PR_ERROR;
	}

	source_base_lid = ntohs(p_source_rec->lid);
	source_last_lid = source_base_lid + pow(2,p_source_rec->lmc) -1;


	for (i = 0; i < guid_to_lid_count; i++) {
		uint16_t dest_base_lid = 0;
		uint16_t dest_last_lid = 0;
		uint16_t source_lid = 0, dest_lid = 0;

		ep_guid_to_lid_tbl_rec* p_dest_rec = p_guid_to_lid_tbl[i];
		dest_base_lid = ntohs(p_dest_rec->lid);
		dest_last_lid = dest_base_lid + pow(2, p_dest_rec->lmc) - 1;

		for(source_lid = source_base_lid ; source_lid<=source_last_lid; ++source_lid){
			for(dest_lid = dest_base_lid ; dest_lid<=dest_last_lid; ++dest_lid){
				ssa_path_parms path_prm;

				path_prm.from_guid = port_guid; 
				path_prm.from_lid = htons(source_lid); 
				path_prm.to_guid = p_dest_rec->guid;
				path_prm.to_lid = htons(dest_lid);

				if(SSA_PR_SUCCESS == ssa_pr_path_params(p_ssa_db_diff,p_source_rec,p_dest_rec,&path_prm)){
					ssa_path_parms revers_path_prm;

					revers_path_prm.from_guid = path_prm.to_guid;
					revers_path_prm.from_lid = path_prm.to_lid; 
					revers_path_prm.to_guid = path_prm.from_guid;
					revers_path_prm.to_lid = path_prm.from_lid;

					ssa_pr_status_t revers_path_res = ssa_pr_path_params(p_ssa_db_diff,p_dest_rec,p_source_rec,&revers_path_prm) ;

					path_prm.reversible = SSA_PR_SUCCESS == revers_path_res ;
					if(NULL!=dump_clbk)
						dump_clbk(&path_prm);
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
		const uint16_t lid,
		const int port_num)
{
	size_t i = 0;
	struct ep_port_tbl_rec  *p_port_tbl = 
		(struct *ep_port_tbl_rec)p_ssa_db_diff->p_tables[SSA_TABLE_ID_PORT];
	const size_t count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_PORT);
	const be16_t port_lid = htons(lid);

	for (i = 0; i < count && p_port_tbl[i].port_lid!=port_lid && port_num != p_port_tbl[i].port_num ); i++) {
		/* code */
	}
}

static ep_link_tbl_rec* find_link(const struct ssa_db_diff* p_ssa_db_diff,
		const uint16_t lid,
		const int port_num)
{
	size_t i = 0;
	struct ep_link_tbl_rec  *p_link_tbl = 
		(struct *ep_link_tbl_rec)p_ssa_db_diff->p_tables[SSA_TABLE_ID_LINK];
	const size_t link_count = get_dataset_count(p_ssa_db_diff,SSA_TABLE_ID_LINK);
	const be16_t from_lid = htons(lid);

	for (i = 0; i < link_count && from_lid!=p_link_tbl[i].from_lid && port_num != p_link_tbl[i].from_port_num; i++);
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
		const ep_guid_to_lid_tbl_rec *guid_to_lid_rec = find_guid_to_lid_rec_by_lid(p_ssa_db_diff,lid);

		if(NULL == link || NULL == guid_to_lid_rec)
			return SSA_PR_ERROR;

		port = find_port(p_ssa_db_diff,lid,port_num);
		if(port == dest_port)
			break;

		if(!guid_to_lid_rec->is_switch)
			return SSA_PR_ERROR;

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

static void ssa_pr_path_output(const ssa_path_parms *p_path_prm)
{
	printf("source guid: 0x%" PRIx64 " source lid: %"SCNu16" dest guid: 0x%" PRIx64 " dest lid: %"SCNu16" mtu: %u rate: %u reversible: %u\n",
			ntohll(p_path_prm->from_guid),ntohs(p_path_prm->from_lid),ntohll(p_path_prm->to_guid),ntohs(p_path_prm->to_lid),
			p_path_prm->mtu,p_path_prm->rate,p_path_prm->reversible);

}
