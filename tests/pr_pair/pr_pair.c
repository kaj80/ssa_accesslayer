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
#include <linux/limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <ssa_db_helper.h>

static void print_usage(FILE* file,const char* name)
{
	fprintf(file, "Usage: %s [-o output folder] input folder\n",name);
}

static int is_dir_exist(const char* path)
{
	DIR* dir = opendir(path);
	if(dir){
		closedir(dir);
		dir = NULL;
		return 1;
	}
	return 0;
}

static void print_memory_usage(const char* prefix)
{
	char buf[30];
	snprintf(buf, 30, "/proc/%u/statm", (unsigned)getpid());
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

int main(int argc,char *argv[])
{
	int opt;
	int index =0;
	char output_path[PATH_MAX]={};
	char input_path[PATH_MAX]={};
	struct ssa_db_diff *db_diff = NULL; 
	clock_t start, end;
	double cpu_time_used;


	while ((opt = getopt(argc, argv, "o:h?")) != -1) {
		switch (opt) {
			case 'o':
				strncpy(output_path,optarg,PATH_MAX);
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
	}

	if (argc == optind ) {
		fprintf(stderr,"Not enough input arguments\n");
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}else if(argc == (optind+1)){
		strncpy(input_path,argv[optind],PATH_MAX);
	}else{
		fprintf(stderr,"Too mutch input arguments\n");
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!is_dir_exist(input_path)){
		fprintf(stderr,"Directory does not exist: %s\n",input_path);
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!strlen(output_path)){
		sprintf(output_path,"%s.output",input_path);
	}else if(is_dir_exist(output_path)){
		char command[1024] ={};
		sprintf(command,"rm -rf %s",output_path);
		system(command);
	}

	if (mkdir(output_path, S_IRWXU|S_IRGRP|S_IXGRP ) != 0 && errno != EEXIST){
		fprintf(stderr,"Directory creation is failed: %s\n",output_path);
		print_usage(stderr,argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("Input path: %s\n",input_path);
	printf("Output path: %s\n",output_path);

	print_memory_usage("Memory usage before the database loading: ");

	start = clock();
	db_diff= ssa_db_load(input_path);
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	if(NULL != db_diff){
		printf("A database is loaded successfully.\n");
		printf("Loading cpu time: %.5f sec.\n",cpu_time_used);
		print_memory_usage("Memory usage after the database loading: ");
	}else{
		fprintf(stderr,"Database loading is failed.\n");
		exit(EXIT_FAILURE);
	}

	start = clock();
	ssa_db_save(output_path, db_diff,0);
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Saving cpu time: %.5f sec.\n",cpu_time_used);

	ssa_db_diff_destroy(db_diff);
	db_diff = NULL;

	return 0;
}
