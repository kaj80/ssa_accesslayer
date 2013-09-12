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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <stdint.h>
#include <byteswap.h>

#include <glib.h>

#include <ssa_smdb.h>
#include <ssa_db_helper.h>
#include <ssa_path_record.h>

#define GUIDS_CHUNK 1024

static void print_usage(FILE *file,const char *name)
{
	fprintf(file,"Usage: %s [-h] [-o output file] [-n number | -f file name | -a] [-l | -g]  input folder\n", name);
	fprintf(file,"\t-h\t\t-Print this help\n");
	fprintf(file,"\t-o\t\t-Output file location. If ommited, stdout is used\n");
	fprintf(file,"\t-f\t\t-Input file location. One ID per line\n");
	fprintf(file,"\t-n\t\t-Input ID\n");
	fprintf(file,"\t-a\t\t-Use all possible IDs. It's a default parameter.\n");
	fprintf(file,"\t-l\t\t-Input ID is LID\n");
	fprintf(file,"\t-g\t\t-Input ID is GUID. It's a default parameter\n");
	fprintf(file,"\tinput folder\t-SMDB database\n");
}

static int is_dir_exist(const char* path)
{
	DIR *dir = opendir(path);

	if(dir) {
		closedir(dir);
		dir = NULL;
		return 1;
	}
	return 0;
}

static int is_file_exist(const char *path)
{
	FILE *file;

	if (file = fopen(path, "r")) {
		fclose(file);
		return 1;
	}
	return 0;
}

static void print_memory_usage(const char* prefix)
{
	char buf[30];
	
	snprintf(buf,30,"/proc/%u/statm",(unsigned)getpid());
	FILE* pf = fopen(buf, "r");
	if (pf) {
		unsigned size; //       total program size
		unsigned resident;//   resident set size
		unsigned share;//      shared pages
		unsigned text;//       text (code)
		unsigned lib;//        library
		unsigned data;//       data/stack
		unsigned dt;//         dirty pages (unused in Linux 2.6)
		fscanf(pf, "%u" /* %u %u %u %u %u"*/, &size/*, &resident, &share, &text, &lib, &data*/);
		printf("%s %u MB mem used\n",prefix, size / (1024.0));
	}
	fclose(pf);
}

static size_t get_dataset_count(const struct ssa_db_smdb *p_ssa_db_smdb,
		unsigned int table_id)
{
	const struct db_dataset *p_dataset = &p_ssa_db_smdb->db_tables[table_id];
	return ntohll(p_dataset->set_count);
}

struct input_prm
{
	char db_path[PATH_MAX];
	char dump_path[PATH_MAX];
	char input_path[PATH_MAX];
	uint64_t id;
	uint8_t whole_world;
	uint8_t is_guid;
};

static void print_input_prm(const struct input_prm *prm)
{
	printf("SMDB database path: %s\n",prm->db_path);
	printf("Dump to : %s\n",strlen(prm->db_path)? prm->db_path: "stdout");
	if(prm->id) {
		if(prm->is_guid) {
			printf("Input GUID: 0x%"PRIx64"\n",prm->id);
			return;
		} else {
			printf("Input LID: 0x%"PRIx16"\n",prm->id);
			return;
		}
	}
	if(prm->whole_world) {
		printf("Compute \"whole world\" path records.\n");
		return;
	}
}

static GPtrArray *init_pr_path_container()
{
	return g_ptr_array_new_with_free_func(g_free);
}

static gint path_compare(gconstpointer a,gconstpointer b)
{
	ssa_path_parms_t *p_path_a = *(ssa_path_parms_t **)a;
	ssa_path_parms_t *p_path_b = *(ssa_path_parms_t **)b;

	uint16_t from_lid_a = ntohs(p_path_a->from_lid);
	uint16_t from_lid_b = ntohs(p_path_b->from_lid);
	uint16_t to_lid_a = ntohs(p_path_a->to_lid);
	uint16_t to_lid_b = ntohs(p_path_b->to_lid);
	int diff_from = from_lid_a - from_lid_b;
	int diff_to = to_lid_a - to_lid_b;

	return diff_from ? diff_from : diff_to;
}

static const struct ep_port_tbl_rec* find_port(const struct ssa_db_smdb* p_ssa_db_smdb,
		const be16_t lid)
{
	size_t i = 0;
	const struct ep_port_tbl_rec  *p_port_tbl = 
		(const struct ep_port_tbl_rec*)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_PORT];
	const size_t count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_PORT);

	for (i = 0; i < count; i++){
		if(p_port_tbl[i].port_lid==lid)
			return p_port_tbl+i;
	}
	return NULL;
}

static const struct ep_guid_to_lid_tbl_rec *find_guid_to_lid_rec_by_lid(const struct ssa_db_smdb* p_ssa_db_smdb,
		const be16_t lid)
{
	size_t i =0;
	const struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl = 
		(struct ep_guid_to_lid_tbl_rec *)p_ssa_db_smdb->p_tables[SSA_TABLE_ID_GUID_TO_LID];
	const size_t count = get_dataset_count(p_ssa_db_smdb,SSA_TABLE_ID_GUID_TO_LID);

	for (i = 0; i < count; i++) {
		if (lid == p_guid_to_lid_tbl[i].lid) 
			return p_guid_to_lid_tbl+i;
	}
	return NULL;
}
static void dump_pr(GPtrArray *path_arr,struct ssa_db_smdb *p_smdb,FILE *fd)
{
	guint i = 0;
	uint16_t prev_lid = 0;
	short first_line = 1;

	g_ptr_array_sort(path_arr,path_compare);

	for (i = 0; i < path_arr->len; i++) {
		ssa_path_parms_t *p_path_prm = g_ptr_array_index(path_arr,i);

		if(prev_lid != p_path_prm->from_lid) {
			const struct ep_port_tbl_rec *p_port_rec = find_port(p_smdb,p_path_prm->from_lid);
			const struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_rec = find_guid_to_lid_rec_by_lid(p_smdb,p_path_prm->from_lid);

			assert(p_port_rec && p_guid_to_lid_rec );

			prev_lid = p_path_prm->from_lid;
			if(first_line)
				first_line = 0;
			else
				fprintf(fd,"\n");

			fprintf(fd,"%s 0x%016"PRIx64", base LID %"SCNu16", port %u\n",!p_guid_to_lid_rec->is_switch?"Channel Adapter":"Switch",
					ntohll(p_guid_to_lid_rec->guid),ntohs(p_guid_to_lid_rec->lid),p_port_rec->port_num);
			fprintf(fd,"# LID  : SL : MTU : RATE\n");
		}
		fprintf(fd,"0x%04"SCNx16" : %3u : %3u : %3u\n",ntohs(p_path_prm->to_lid),0,p_path_prm->mtu,p_path_prm->rate);
	}
}

static void ssa_pr_path_output(const ssa_path_parms_t *p_path_prm, void *prm)
{
	ssa_path_parms_t *p_my_path = NULL;
	GPtrArray *path_arr = (GPtrArray *)prm;

	p_my_path =  (void*)g_malloc(sizeof *p_my_path);

	memcpy(p_my_path,p_path_prm,sizeof(*p_my_path));
	g_ptr_array_add(path_arr,p_my_path);
}

static struct ssa_db_smdb *load_smdb(const char *path)
{
	struct ssa_db_smdb *db_diff = NULL; 
	clock_t start, end;
	double cpu_time_used;

	print_memory_usage("Memory usage before the database loading: ");

	start = clock();
	db_diff = ssa_db_load(path,SSA_DB_HELPER_DEBUG);
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	if(NULL != db_diff) {
		printf("A database is loaded successfully.\n");
		printf("Loading cpu time: %.5f sec.\n",cpu_time_used);
		print_memory_usage("Memory usage after the database loading: ");
	} else {
		fprintf(stderr,"Database loading is failed.\n");
	}
	return db_diff;
}

static void destroy_smdb(struct ssa_db_smdb *db_diff)
{
	ssa_db_smdb_destroy(db_diff);
	printf("smdb database is destroyed\n");
}

static size_t read_ids_from_file(const char *path, GArray *arr)
{
	FILE* fd = NULL;
	size_t count = 0;
	uint64_t id = 0;

	if(0 == strlen(path))
		goto Exit;

	fd = fopen(path,"r");
	if(!fd) {
		fprintf(stderr,"Can't open file for reading: %s\n",path);
		goto Exit;
	}

	while(1 == fscanf(fd,"0x%"PRIx64"",&id)) {
		g_array_append_val(arr,id);
		count++;
	}	

Exit:
	if(!fd) {
		fclose(fd);
		fd = NULL;
	}
	return count;
}

static void remove_duplicates(GArray *p_arr)
{
	guint i = 0;
	guint last = 0;

	for(i = 1; i < p_arr->len; ++i) {
		uint64_t v_i = g_array_index(p_arr,uint64_t,i);
		uint64_t v_last = g_array_index(p_arr,uint64_t,last);
		if(v_i != v_last)
			g_array_index(p_arr,uint64_t,++last) = v_i;
	}

	g_array_remove_range(p_arr,last,i-last);
}

static int compare_ints(uint64_t a,uint64_t b)
{
	return a - b;
}

static size_t get_input_guids(const struct input_prm *p_prm,
		struct ssa_db_smdb *p_db,
		GArray* p_arr)
{
	assert(p_prm && p_arr && p_db);

	if(p_prm->is_guid) {
		/*There is only one guid*/
		g_array_append_val(p_arr,p_prm->id);
		return 1;	 
	} else if(p_prm->whole_world) {
		size_t i = 0;

		const struct ep_guid_to_lid_tbl_rec *p_guid_to_lid_tbl =
			(struct ep_guid_to_lid_tbl_rec *)p_db->p_tables[SSA_TABLE_ID_GUID_TO_LID];
		const size_t count = get_dataset_count(p_db,SSA_TABLE_ID_GUID_TO_LID);

		for (i = 0; i < count; i++) { 
			uint64_t id = ntohll(p_guid_to_lid_tbl[i].guid);
			g_array_append_val(p_arr,id);
		}
	} else if(strlen(p_prm->input_path)>0) {
		const size_t tmp = read_ids_from_file(p_prm->input_path,p_arr);

		if(0 == tmp) {
			fprintf(stderr,"Can't read ids from file: %s\n",p_prm->input_path);
			return 0;
		}
	}

	g_array_sort(p_arr, (GCompareFunc)compare_ints);
	remove_duplicates(p_arr);

	return 0;
}

static int run_pr_calculation(struct input_prm* p_prm)
{
	short dump_to_stdout = 1;
	FILE* fd_dump = NULL;
	struct ssa_db_smdb *p_db_diff = NULL;
	be64_t *p_guids = NULL;
	size_t count_guids = 0;
	GPtrArray *path_arr = NULL;
	GArray *guids_arr = NULL;
	guint i = 0;
	int res = 0;

	if(ssa_open_log1(SSA_ACCESS_LAYER_OUTPUT_FILE)) {
		fprintf(stderr,"Can't open log file: %s\n",SSA_ACCESS_LAYER_OUTPUT_FILE);
		return -1;
	}

	if(strlen(p_prm->dump_path) > 0) {
		fd_dump = fopen(p_prm->dump_path,"w");
		if(!fd_dump) {
			fprintf(stderr,"Can't open file for writing: %s\n",p_prm->dump_path);
			return -1;
		}
		dump_to_stdout = 0;
	} else {
		fd_dump = stdout;
		dump_to_stdout = 1;
	}

	p_db_diff = load_smdb(p_prm->db_path);
	if(NULL == p_db_diff){
		fprintf(stderr,"Can't create smdb database from: %s .\n",p_prm->db_path);
		res = -1;
		goto Exit;
	}

	guids_arr = g_array_sized_new(FALSE,TRUE,sizeof(uint64_t),10000);
	if(NULL == guids_arr) {
		fprintf(stderr,"Can't create Glib array for guids.\n");
		res = -1;
		goto Exit;
	}
	get_input_guids(p_prm,p_db_diff,guids_arr);

	path_arr = init_pr_path_container();
	if(NULL == path_arr) {
		fprintf(stderr,"Can't create a Glib array\n");
		res = -1;
		goto Exit;
	}

	for(i = 0; i < guids_arr->len; ++i) {
		be64_t guid = htonll(g_array_index(guids_arr,uint64_t,i));
		ssa_pr_status_t res = SSA_PR_SUCCESS;

		ssa_log(SSA_LOG_ALL,"Input guid: 0x%-16"PRIx64"\n",ntohll(guid));	
		res = ssa_pr_half_world(p_db_diff,guid,ssa_pr_path_output,path_arr);
		if(SSA_PR_SUCCESS != res) {
			fprintf(stderr,"Path record algorithm is failed. Input guid: host order -  0x%"PRIx64" network order - 0x%"PRIx64"\n",ntohll(guid),guid);
			res = -1;
			goto Exit;
		}
	}

	dump_pr(path_arr,p_db_diff,fd_dump);

Exit:
	if(!p_db_diff) {
		destroy_smdb(p_db_diff);
		p_db_diff = NULL;
	}

	if(!dump_to_stdout && fd_dump) {
		fclose(fd_dump);
		fd_dump = NULL;
	}
	if(!p_guids) {
		free(p_guids);
		p_guids = NULL;
	}
	if(!path_arr) {
		g_ptr_array_free (path_arr,TRUE);
		path_arr = NULL;
	}
	if(!guids_arr) {
		g_array_free(guids_arr,FALSE);
		guids_arr = NULL;
	}
	ssa_close_log1();
	return res;
}

int main(int argc,char *argv[])
{
	int opt = 0;
	int index = 0;
	struct input_prm prm;
	char dump_path[PATH_MAX] = {};
	char input_path[PATH_MAX] = {};
	char db_path[PATH_MAX] = {};
	short use_output_opt = 0;
	short use_all_opt = 0;
	short use_file_opt = 0;
	short use_single_id_opt = 0;
	short use_guid_opt = 0;
	short use_lid_opt = 0;
	short err_opt = 0;
	uint64_t id = 0; 
	char id_string_val[PATH_MAX] = {};

	memset(&prm,'\0',sizeof(prm));

	while ((opt = getopt(argc, argv, "glan:f:o:h?")) != -1) {
		switch (opt) {
			case 'o':
				use_output_opt = 1;
				strncpy(dump_path,optarg,PATH_MAX);
				break;
			case 'a':
				use_all_opt = 1;
				prm.whole_world = 1;
				err_opt = use_file_opt || use_single_id_opt;
				break;
			case 'n':
				use_single_id_opt = 1;
				err_opt = use_file_opt || use_all_opt;
				if(!err_opt){
					strncpy(id_string_val,optarg,PATH_MAX);
				}
				break;
			case 'f':
				use_file_opt = 1;
				err_opt = use_single_id_opt || use_all_opt;
				break;
			case 'l':
				use_lid_opt = 1;
				err_opt = use_guid_opt;
				break;
			case 'g':
				use_guid_opt = 1;
				err_opt = use_lid_opt;
				break;
			case '?':
			case 'h':
				print_usage(stdout,argv[0]);
				return 0;
				break;
			default: /* '?' */
				if(isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr,
							"Unknown option character `\\x%x'.\n",
							optopt);
				print_usage(stderr,argv[0]);
				exit(EXIT_FAILURE);
		}
		if(err_opt) {
			fprintf (stderr, "Incompatible options.\n");
			print_usage(stderr,argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (argc == optind ) {
		fprintf(stderr,"Not enough input arguments\n");
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}else if(argc == (optind+1)) {
		strncpy(db_path,argv[optind],PATH_MAX);
	}else{
		fprintf(stderr,"Too mutch input arguments\n");
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!use_file_opt && !use_single_id_opt)
		/*It's a default option*/
		use_all_opt = 1;

	prm.whole_world = use_all_opt;

	if(!use_lid_opt && !use_guid_opt)
		/*It's a default option*/
		use_guid_opt = 1;

	if(use_single_id_opt) {
		int res = 0;

		if(strlen(id_string_val) > 2 && '0' == id_string_val[0] && 'x' == id_string_val[1])
			res = sscanf(id_string_val,"0x%"PRIx64,&id);
		else
			res = sscanf(id_string_val,"%"PRIx64,&id);

		if(res != 1) {
			fprintf(stderr,"String : %s can't be converted to numeric value.\n",id_string_val);
			print_usage(stderr,argv[0]);
			exit(EXIT_FAILURE);
		}

		prm.id = id;
		prm.is_guid = use_guid_opt;
	}

	if(!is_dir_exist(db_path)) {
		fprintf(stderr,"Directory does not exist: %s\n",db_path);
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	} else {
		strncpy(prm.db_path,db_path,PATH_MAX);
	}

	prm.dump_path[0] = '\0';
	if(use_output_opt && strcmp(dump_path,"stdout")) {
		if(is_file_exist(dump_path))
			fprintf(stderr,"Dump file will be replaced: %s\n",dump_path);
		strncpy(prm.dump_path,dump_path,PATH_MAX);
	}

	if(use_file_opt)
		if(!is_dir_exist(input_path)) {
			fprintf(stderr,"Directory does not exist: %s\n",input_path);
			print_usage(stderr,argv[0]);
			exit(EXIT_FAILURE);
		} else {
			strncpy(prm.input_path,input_path,PATH_MAX);
		}

	print_input_prm(&prm);

	if(!run_pr_calculation(&prm))
		printf("Path record calculation is succeeded\n");
	else
		printf("Path record calculation is failed\n");

	return 0;
}
