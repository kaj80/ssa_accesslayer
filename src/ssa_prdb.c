/*
 * Copyright (c) 2011-2013 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
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


#include <ssa_db.h>
#include <ssa_prdb.h>
#include <asm/byteorder.h>

static const struct db_table_def def_tbl[] = {
	{ 1, sizeof(struct db_table_def), DBT_TYPE_DATA, 0, { 0,SSA_PR_TABLE_ID, 0 },
		"PR", __constant_htonl(sizeof(struct ep_pr_tbl_rec)), 0 },
	{ 1, sizeof(struct db_table_def), DBT_TYPE_DEF, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF , 0 },
		"PR fields", __constant_htonl(sizeof(struct db_field_def)), __constant_htonl(SSA_PR_TABLE_ID) },
	{ 0 }
};

static const struct db_dataset dataset_tbl[] = {
	{ 1, sizeof(struct db_dataset), 0, 0, { 0, SSA_PR_TABLE_ID, 0 }, 0, 0, 0, 0 },
	{ 0 }
};

static const struct db_dataset field_dataset_tbl[] = {
	{ 1, sizeof(struct db_dataset), 0, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF , 0 }, 0, 0, 0, 0 },
	{0}
};

static const struct db_field_def field_tbl[] = {
	{ 1, 0, DBF_TYPE_NET64, 0, { 0,SSA_PR_TABLE_ID_FIELD_DEF,SSA_PR_FIELD_ID_PR_DGUID}, "guid", __constant_htonl(64), 0 },
	{ 1, 0, DBF_TYPE_NET16, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_DLID }, "lid", __constant_htonl(16), __constant_htonl(64) },
	{ 1, 0, DBF_TYPE_NET16, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_PK }, "pk", __constant_htonl(16), __constant_htonl(80) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_MTU }, "mtu", __constant_htonl(8), __constant_htonl(96) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_RATE }, "rate", __constant_htonl(8), __constant_htonl(104) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_SL }, "sl", __constant_htonl(8), __constant_htonl(112) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID_FIELD_DEF, SSA_PR_FIELD_ID_PR_REVERSIBLE }, "is_reversible", __constant_htonl(8), __constant_htonl(120) },
	{ 0 }
};


/** =========================================================================
 */
struct ssa_db  *ssa_prdb_create(uint64_t num_recs)
{
	struct ssa_db *p_ssa_db = NULL;
	uint64_t num_recs_arr[SSA_PR_TABLE_ID_MAX] = {};
	uint64_t num_field_recs_arr[SSA_PR_TABLE_ID_MAX] = {};
	size_t recs_size_arr[SSA_PR_TABLE_ID_MAX] ={};


	num_recs_arr[SSA_PR_TABLE_ID] = num_recs;
	recs_size_arr[SSA_PR_TABLE_ID] = sizeof(struct ep_pr_tbl_rec);
	num_field_recs_arr[SSA_PR_TABLE_ID] = SSA_PR_FIELDS_ID_MAX;

	p_ssa_db = ssa_db_create(num_recs_arr, recs_size_arr, num_field_recs_arr, SSA_PR_TABLE_ID_MAX);

	ssa_db_init(p_ssa_db, "PRDB", 10 /*just some db_id */, def_tbl, dataset_tbl,
		    field_dataset_tbl, field_tbl);

	return p_ssa_db;
}

