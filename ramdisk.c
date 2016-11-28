/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fnmatch.h>
#include "rdstructs.h"

int NNODES;
int NBLOCKS;
int persistent = 0;
FILE *fp;
char filename[PATH_MAX];
char a_path[PATH_MAX];

filesystem f;

void nodes_init()
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		f.nodes[i].status = unused;
		f.nodes[i].size = 0;
		f.nodes[i].start_block = -1;
		f.nodes[i].is_dir = 0;
	}
}

void blocks_init()
{
	int i;
	for (i = 0; i < f.nblocks; i++) {
		f.blocks[i].status = unusedblock;
		f.blocks[i].next_block = -1;
	}
}

int path_search(const char *path)
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			return i;
		}
	}

	return -ENOENT;
}

int print_info()
{
	int j, i, start, k=0, h=0;
	for(i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used ) {
			printf("\n\n------File: %s---------\n", f.nodes[i].path);
			start = f.nodes[i].start_block;
			j = 0;
			while(start!=-1) {
			//	printf("blockindex: %d status: %d content: %s\n", start,f.blocks[start].status, f.blocks[start].data);
				start = f.blocks[start].next_block;	
				j+=1;
			}
			printf("------Blocks in file: %d----------\n",j);
			k++;
			h+=j;
		}
	}
	printf("------Total Files: %d----------\n",k);
	printf("------Total Blocks: %d----------\n",h);

	j = 0;
	printf("\n\n------Reading Blocks---\n");
	printf("Blocks: ");
	for(i= 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == usedblock ) {
			//printf("%d, ", i);
			j+=1;
		}
	}
	printf("\n------Total Blocks: %d----------\n\n",j);

	return 0;
}

node* get_free_node()
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == unused ){
			f.nodes[i].status = used;
			return &f.nodes[i];
		}
	}
	//perror("No more nodes left!!");
	return NULL;
}

int get_free_block_index()
{
	int i;
	for (i = 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == unusedblock ){
			f.blocks[i].status = usedblock;
			return i;
		}
	}
	//perror("No more blocks left!!");
	return -1;
}

int hello_init() 
{
	f.block_size = BLOCK_SIZE;
	f.nnodes = NNODES;
	f.nblocks = NBLOCKS;
	
	f.nodes = (node *) malloc(f.nnodes*sizeof(node));
	f.blocks = (block *) malloc(f.nblocks*sizeof(block));
	
	if (persistent == 1) {
		fp = fopen(filename, "r");
		if (fp != NULL) {
			fread(f.nodes, sizeof(node), f.nnodes, fp);
			fread(f.blocks, sizeof(block), f.nblocks, fp);
			//print_info();
		}
		else {
			fp = fopen(filename, "w+");
			nodes_init();
			blocks_init();
			fclose(fp);
		}
	}
	else {
		nodes_init();
		blocks_init();
	}
	
	return 0;
}

void hello_destroy(void *v)
{
	if (persistent == 1) {
		fp = fopen(filename, "w");
		if (fp != NULL) {
			fwrite(f.nodes, sizeof(node), f.nnodes, fp);
			fwrite(f.blocks, sizeof(block), f.nblocks, fp);
			fclose(fp);
		}
		else {
			perror("Couldn't open file in write mode");
		}
	}
	
	free(f.nodes);
	free(f.blocks);
} 

int pathmatch(const char *dir, const char *path)
{
	strcpy(a_path, dir);
	strcat(a_path, "/*");
	return fnmatch(a_path, path, FNM_PATHNAME);
}

int hello_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_size = f.block_size;
		return 0;
	} 
	
	int i = path_search(path);
	if ( i < 0 )
		return i;

	stbuf->st_mode = f.nodes[i].mode;
	stbuf->st_nlink = 1;
	stbuf->st_size = f.nodes[i].size;	
	return 0;
}

int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	int i,index = -1;
	char *dirpath = "";
	
	if (strcmp(path, "/") != 0 ) {
		for (i = 0; i < f.nnodes; i++) {
			if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
				index = i;
				dirpath = f.nodes[i].path;
				break;
			}
		}

		if (index == -1)
			return -ENOENT;
	
	}
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && pathmatch(dirpath, f.nodes[i].path) == 0 ) {
			filler(buf, strrchr(f.nodes[i].path, '/')+1 , NULL, 0);
		}
	}

	//print_info();

	return 0;
}

int hello_open(const char *path, struct fuse_file_info *fi)
{
	if ( path_search(path) == -1 ){
		return -ENOENT;	
	}
	return 0;
}

int hello_create(const char *path, mode_t mode, struct fuse_file_info *fi) 
{
	//open file if it exists. if not the create and open file
	if ( path_search(path) == -ENOENT ) {
		//file doesn't exist
		node* file;
		file = get_free_node();
		if (file == NULL) {
			//perror("Cannot create file: No nodes left.");
			return -EDQUOT;
		}
		
		file->mode = mode;
		strcpy(file->path, path);	
		
	}
	
	return 0;
}

int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes read
	int fileblock, size2,i;

	i = path_search(path);
	if ( i < 0 )
		return i;
	
	if ( size > f.nodes[i].size ) {
		// cant read more than the size of file
		size = f.nodes[i].size;
	}

	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;
	
	if ( fileblock == -1 )
		return 0;
	
	//seek to the offset with fileblock pointer
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual read
	
	// read partial block
	if ( offset > 0 ) {
		memcpy(buf, f.blocks[fileblock].data + offset , size2-offset);
		fileblock = f.blocks[fileblock].next_block;
	}
	
	// read all other whole blocks
	while ( offset < size ) {	
		if ( offset + size2 > size ){
			size2 = size - offset;
		}

		if ( fileblock == -1 ) {
			//perror("read_error:this is not supposed to happen\n");
			break;
		}
		
		memcpy(buf + offset, f.blocks[fileblock].data , size2);
		fileblock = f.blocks[fileblock].next_block;
		offset += size2;
	}			
	return size;
}

int hello_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes written
	int fileblock, last, size2, i;
	i = path_search(path);
	if ( i < 0 )
		return i;
			
	f.nodes[i].size = offset;
	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;

	// file is new, link a block
	if ( fileblock == -1 && size > 0) {
		fileblock = get_free_block_index();
		if (fileblock == -1) {
			//perror("Cannot write to file: No space left.");
			f.nodes[i].size = 0;
			return -EDQUOT;
		}
		f.nodes[i].start_block = fileblock;
	}

	//seek to the offset with fileblock pointer
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual write
	
	// write partial block
	if ( offset > 0 ) {
		memcpy(f.blocks[fileblock].data + offset, buf, size2-offset);
		last = fileblock;
		fileblock = f.blocks[fileblock].next_block;
		offset = size2 - offset;
	}

	// write to all other whole blocks
	while ( offset < size ) {	
		if ( offset + size2 > size ) {
			size2 = size - offset;
		}
		
		if ( fileblock == -1 ) {
			fileblock = get_free_block_index();
			if (fileblock == -1) {
				//perror("Cannot write to file: No space left.");
				errno = -EDQUOT;
				break;
			}
			f.blocks[last].next_block = fileblock;
		}

		memcpy(f.blocks[fileblock].data, buf + offset , size2);
		last = fileblock;
		fileblock = f.blocks[fileblock].next_block;
		offset += size2;
	}
	
	f.nodes[i].size += offset;
	
	return offset;	 
}

int hello_mkdir(const char *path, mode_t mode)
{
	if ( path_search(path) == -ENOENT) {
		//directory doesn't exist
		node* dir;
		dir = get_free_node();
		if (dir == NULL) {
			//perror("Cannot create directory: No space left.");
			return -EDQUOT;
		}
		
		dir->is_dir = 1;
		dir->mode = S_IFDIR|mode;
		dir->size = f.block_size;
		strcpy(dir->path, path);	
		return 0;
	}
	
	return -EEXIST;
}

int free_block(int index)
{
	int next;
	f.blocks[index].status = unusedblock;
	next = f.blocks[index].next_block;
	f.blocks[index].next_block = -1;
	return next;
}

int hello_truncate(const char *path, off_t offset)
{
	int fileblock, size2, i;

	i = path_search(path);
	if ( i < 0 )
		return i;
			
	if ( offset >= f.nodes[i].size ) {
		//see man 2 truncate, if offset > size then fill with /0 characters
		return 0;
	}

	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;
	
	//seek to the offset
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual truncate

	// truncate partial block
	if ( offset > 0 ) {
		memset(f.blocks[fileblock].data + offset, '\0', size2-offset);
		fileblock = f.blocks[fileblock].next_block;
		offset = size2 - offset;
	}
	
	//truncate whole blocks
	while ( fileblock != -1 ) {
		fileblock = free_block(fileblock);
	}
	
	if (offset == 0) {
		f.nodes[i].start_block = -1;	
	}
	
	f.nodes[i].size = offset;
	return 0;	
}

int hello_unlink(const char *path)
{
	int status, i;
	
	status = hello_truncate(path, 0);
	if (status < 0) {
		return status;
	}
	
	i = path_search(path);		
	f.nodes[i].start_block = -1;
	f.nodes[i].status = unused;
		
	return 0;
}

int hello_rmdir(const char *path)
{
	int i, j, index = -1;
	
	if (strcmp(path, "/") == 0) {
		return -EACCES;
	}

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			index = i;
			break;
		}
	}

	if ( index == -1 ) {
		return -ENOENT;
	}

	for (j = 0; j < f.nnodes; j++) {
		if ( f.nodes[j].status == used && f.nodes[j].is_dir == 0 && pathmatch(path, f.nodes[j].path) == 0 ) {
			return -ENOTEMPTY;
		}
	}

	f.nodes[i].status = unused;
		
	return 0;

}

int hello_rename(const char *path, const char *newpath)
{
	// works only for files and not directories
	int i = path_search(path);
	if ( i < 0 )
		return i;
	
	if ( f.nodes[i].is_dir == 0) {
		int j = path_search(newpath);
		if ( j>= 0 && f.nodes[j].is_dir == 0) {
			hello_unlink(newpath);
		}	
		strcpy(f.nodes[i].path, newpath);	
	}
	
	return 0;
}

int hello_opendir(const char *path, struct fuse_file_info *fu)
{
	int i;
	if (strcmp("/", path) == 0) {
		return 0;
	}

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			return 0;
		}
	}

	return -ENOENT;
}

int hello_flush(const char *path, struct fuse_file_info *f)
{
	return 0;
}

struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.opendir	= hello_opendir,
	.mkdir		= hello_mkdir,
	.rmdir		= hello_rmdir,
	.create		= hello_create,
	.truncate 	= hello_truncate,
	.open		= hello_open,
	.read		= hello_read,
	.write 		= hello_write,
	.unlink		= hello_unlink,
	.flush		= hello_flush,
	.destroy 	= hello_destroy,
	.rename 	= hello_rename
};

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <mount-directory> <sizeinMB> [<disk-image>]\n", argv[0]);
		return -1;
	}

	if (argc == 4) {
		strcpy(filename, argv[3]);
		persistent = 1;
	}

	size_t size_bytes = atoi(argv[2])*1000000;
	NBLOCKS = size_bytes/(sizeof(node) + sizeof(block));
	NNODES = NBLOCKS;
	
	//size_t storage = NBLOCKS*sizeof(block);
	//fprintf(stderr,"number of blocks: %d\n", NBLOCKS);
	//fprintf(stderr,"number of nodes: %d\n", NNODES);
	//fprintf(stderr,"Total space for storage: %lu\n", storage);

	hello_init();

	argc = 2;
	return fuse_main(argc, argv, &hello_oper, NULL);
}
