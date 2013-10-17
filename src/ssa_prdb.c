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

#include <ssa_prdb.h>
#include <asm/byteorder.h>

static const struct db_table_def def_tbl[] = {
	{ 1, sizeof(struct db_table_def), DBT_TYPE_DATA, 0, { 0,SSA_PR_TABLE_ID, 0 },
		"PR", __constant_htonl(sizeof(struct ep_pr_tbl_rec)), 0 },
	{ 0 }
};

static const struct db_dataset dataset_tbl[] = {
	{ 1, sizeof(struct db_dataset), 0, 0, { 0, SSA_PR_TABLE_ID, 0 }, 0, 0, 0, 0 },
	{ 0 }
};

static const struct db_field_def field_tbl[] = {
	{ 1, 0, DBF_TYPE_NET64, 0, { 0,SSA_PR_TABLE_ID,SSA_PR_FIELD_ID_PR_DGUID}, "guid", __constant_htonl(64), 0 },
	{ 1, 0, DBF_TYPE_NET16, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_DLID }, "lid", __constant_htonl(16), __constant_htonl(64) },
	{ 1, 0, DBF_TYPE_NET16, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_PK }, "pk", __constant_htonl(16), __constant_htonl(80) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_MTU }, "mtu", __constant_htonl(8), __constant_htonl(96) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_RATE }, "rate", __constant_htonl(8), __constant_htonl(104) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_SL }, "sl", __constant_htonl(8), __constant_htonl(112) },
	{ 1, 0, DBF_TYPE_U8, 0, { 0, SSA_PR_TABLE_ID, SSA_PR_FIELD_ID_PR_REVERSIBLE }, "is_reversible", __constant_htonl(8), __constant_htonl(120) },
	{ 0 }
};

struct db_field {
	enum ssa_prdb_table_id	table_id;
	uint8_t	fields_num;
};

static const struct db_field field_per_table[] = {
	{ SSA_PR_TABLE_ID, SSA_PR_FIELDS_ID_MAX},
	{ 0 }
};

static
void ssa_prdbdb_def_init(struct db_def * p_db_def,
			     uint8_t version, uint8_t size,
			     uint8_t db_id, uint8_t table_id,
			     uint8_t field_id, const char * name,
			     uint32_t table_def_size)
{
	p_db_def->version		= version;
	p_db_def->size			= size;
	p_db_def->id.db			= db_id;
	p_db_def->id.table		= table_id;
	p_db_def->id.field		= field_id;
	strcpy(p_db_def->name, name);
	p_db_def->table_def_size	= htonl(table_def_size);
}

/** =========================================================================
 */
static
void ssa_prdb_dataset_init(struct db_dataset * p_dataset,
			      uint8_t version, uint8_t size,
			      uint8_t access, uint8_t db_id,
			      uint8_t table_id, uint8_t field_id,
			      uint64_t epoch, uint64_t set_size,
			      uint64_t set_offset, uint64_t set_count)
{
	p_dataset->version	= version;
	p_dataset->size		= size;
	p_dataset->access	= access;
	p_dataset->id.db	= db_id;
	p_dataset->id.table	= table_id;
	p_dataset->id.field	= field_id;
	p_dataset->epoch	= htonll(epoch);
	p_dataset->set_size	= htonll(set_size);
	p_dataset->set_offset	= htonll(set_offset);
	p_dataset->set_count	= htonll(set_count);
}

/** =========================================================================
 */
static
void ssa_prdb_table_def_insert(struct db_table_def * p_tbl,
				  struct db_dataset * p_dataset,
				  uint8_t version, uint8_t size,
				  uint8_t type, uint8_t access,
				  uint8_t db_id, uint8_t table_id,
				  uint8_t field_id, const char * name,
				  uint32_t record_size, uint32_t ref_table_id)
{
	struct db_table_def db_table_def_rec;

	memset(&db_table_def_rec, 0, sizeof(db_table_def_rec));

	db_table_def_rec.version	= version;
	db_table_def_rec.size		= size;
	db_table_def_rec.type		= type;
	db_table_def_rec.access		= access;
	db_table_def_rec.id.db		= db_id;
	db_table_def_rec.id.table	= table_id;
	db_table_def_rec.id.field	= field_id;
	strcpy(db_table_def_rec.name, name);
	db_table_def_rec.record_size	= htonl(record_size);
	db_table_def_rec.ref_table_id	= htonl(ref_table_id);

	memcpy(&p_tbl[ntohll(p_dataset->set_count)], &db_table_def_rec,
	       sizeof(*p_tbl));
	p_dataset->set_count = htonll(ntohll(p_dataset->set_count) + 1);
	p_dataset->set_size = htonll(ntohll(p_dataset->set_size) + sizeof(*p_tbl));
}

/** =========================================================================
 */
static
void ssa_prdb_field_def_insert(struct db_field_def * p_tbl,
				  struct db_dataset * p_dataset,
				  uint8_t version, uint8_t type,
				  uint8_t db_id, uint8_t table_id,
				  uint8_t field_id, const char * name,
				  uint32_t field_size, uint32_t field_offset)
{
	struct db_field_def db_field_def_rec;

	memset(&db_field_def_rec, 0, sizeof(db_field_def_rec));

	db_field_def_rec.version	= version;
	db_field_def_rec.type		= type;
	db_field_def_rec.id.db		= db_id;
	db_field_def_rec.id.table	= table_id;
	db_field_def_rec.id.field	= field_id;
	strcpy(db_field_def_rec.name, name);
	db_field_def_rec.field_size	= htonl(field_size);
	db_field_def_rec.field_offset	= htonl(field_offset);

	memcpy(&p_tbl[ntohll(p_dataset->set_count)], &db_field_def_rec,
	       sizeof(*p_tbl));
	p_dataset->set_count = htonll(ntohll(p_dataset->set_count) + 1);
	p_dataset->set_size = htonll(ntohll(p_dataset->set_size) + sizeof(*p_tbl));
}

/** =========================================================================
 */
static void ssa_prdb_tables_init(struct ssa_prdb * p_prdb)
{
	const struct db_table_def *p_tbl_def;
	const struct db_dataset *p_dataset;
	const struct db_field_def *p_field_def;
	const struct db_field *p_field;

	/*
	 * db_def initialization
	 */
	ssa_prdbdb_def_init(&p_prdb->db_def,
				0, sizeof(p_prdb->db_def),
				12 /* just some db_id */, 0, 0, "SMDB",
				sizeof(*p_prdb->p_def_tbl));

	/*
	 * Definition tables dataset initialization
	 */
	ssa_prdb_dataset_init(&p_prdb->db_table_def,
				 0, sizeof(p_prdb->db_table_def),
				 0, 0, SSA_PR_TABLE_ID_TABLE_DEF, 0,
				 0, 0, 0, 0);

	p_prdb->p_def_tbl = (struct db_table_def *)
		malloc(sizeof(*p_prdb->p_def_tbl) * SSA_PR_TABLE_ID_MAX);
	if (!p_prdb->p_def_tbl) {
		/* add handling memory allocation failure */
	}

	/* adding table definitions */
	for (p_tbl_def = def_tbl; p_tbl_def->version; p_tbl_def++)
		ssa_prdb_table_def_insert(p_prdb->p_def_tbl,
					     &p_prdb->db_table_def,
					     p_tbl_def->version, p_tbl_def->size,
					     p_tbl_def->type, p_tbl_def->access,
					     p_tbl_def->id.db, p_tbl_def->id.table,
					     p_tbl_def->id.field, p_tbl_def->name,
					     ntohl(p_tbl_def->record_size),
					     ntohl(p_tbl_def->ref_table_id));

	/* data tables datasets initialization */
	for (p_dataset = dataset_tbl; p_dataset->version; p_dataset++)
		ssa_prdb_dataset_init(&p_prdb->db_tables[p_dataset->id.table],
					 p_dataset->version, p_dataset->size,
					 p_dataset->access, p_dataset->id.db,
					 p_dataset->id.table, p_dataset->id.field,
					 p_dataset->epoch, p_dataset->set_size,
					 p_dataset->set_offset,
					 p_dataset->set_count);

	/* field tables initialization */
	for (p_field = field_per_table; p_field->table_id; p_field++) {
		p_prdb->p_tables[p_field->table_id] =
			malloc(sizeof(struct db_field_def) * p_field->fields_num);
		if (!p_prdb->p_tables[p_field->table_id]) {
			/* add handling memory allocation failure */
		}
		for (p_field_def = field_tbl; p_field_def->version; p_field_def++) {
			if (p_field_def->id.table == p_field->table_id)
				ssa_prdb_field_def_insert(p_prdb->p_tables[p_field->table_id],
							     &p_prdb->db_tables[p_field->table_id],
							     p_field_def->version, p_field_def->type,
							     p_field_def->id.db, p_field_def->id.table,
							     p_field_def->id.field, p_field_def->name,
							     ntohl(p_field_def->field_size),
							     ntohl(p_field_def->field_offset));
		}
	}
}

/** =========================================================================
 */
struct ssa_prdb ssa_prdb_init(uint64_t num_recs)
{
	struct ssa_prdb  prdb;

	ssa_prdb_tables_init(&prdb);

	prdb.p_tables[SSA_PR_TABLE_ID] =
		malloc(sizeof(struct ep_pr_tbl_rec) * num_recs);

	if (!prdb.p_tables[SSA_PR_TABLE_ID]) {
		/* TODO: add handling memory allocation failure */
	}

	return prdb;
}

/** =========================================================================
 */
void ssa_prdbdb_destroy(struct ssa_prdb * p_prdb)
{
	int i;

	if (!p_prdb)
		return;

	for (i = 0; i < SSA_PR_TABLE_ID_MAX; i++)
		free(p_prdb->p_tables[i]);
}

