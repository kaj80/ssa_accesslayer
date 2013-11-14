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
#ifndef __SSA_PATH_RECORD_H__
#define __SSA_PATH_RECORD_H__

/*
 * The file contains SSA Access Layer API. 
 */

#include <stdint.h>
#include <byteswap.h>
#include <infiniband/umad.h>
#include <ssa_db.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _ssa_pr_status_t {
    SSA_PR_SUCCESS,
    SSA_PR_ERROR,
	SSA_PR_NO_PATH
} ssa_pr_status_t;

typedef struct ssa_path_parms {
	be64_t from_guid;
	be64_t to_guid;
	be16_t from_lid;
	be16_t to_lid;
	be16_t pkey;
	uint8_t mtu;
	uint8_t rate;
	uint8_t sl;
	uint8_t pkt_life;
	uint8_t reversible;
	uint8_t hops;
} ssa_path_parms_t;


typedef void (*ssa_pr_path_dump_t)(const ssa_path_parms_t*,void*);


extern void * ssa_pr_create_context(FILE* log_fd, int log_level);
extern void ssa_pr_destroy_context(void * ctx);

/* ssa_pr_compute_half_world function computes "half world" path records 
 * 					for given GUID. As result the function creates prdb 
 * 					database. A caller is responsible for destroy the
 * 					database.
 * @return value	- pointer to prdb database. NULL in vase of failure.
 * @p_ssa_db_smdb	- input smdb database
 * @p_ctnx			- context. The context is used for storing data between different
 * 						functions call. A caller has to create a context before first call
 * 						and destroy it at end.
 * @port_guid		- input GUID
 * 
 * */
extern struct ssa_db *ssa_pr_compute_half_world(struct ssa_db *p_ssa_db_smdb, 
		void *p_ctnx,
		be64_t port_guid);

extern ssa_pr_status_t ssa_pr_half_world(struct ssa_db* p_ssa_db_smdb, 
		void * context,
		be64_t port_guid,
		ssa_pr_path_dump_t dump_clbk,
		void* clbk_prm);

extern ssa_pr_status_t ssa_pr_whole_world(struct ssa_db* p_ssa_db_smdb, 
		void * context,
		ssa_pr_path_dump_t dump_clbk,
		void* clbk_prm);

#ifdef __cplusplus
}
#endif

#endif /* __SSA_PATH_RECORD_H__ */
