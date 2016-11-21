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
#include "rdstructs.h"

FILE *fp;
char *filename = "ramdisk.save";

filesystem f;

void nodes_init()
{
	printf("----------nodes init\n");
	for (int i = 0; i < f.nnodes; i++) {
		f.nodes[i].status = unused;
		f.nodes[i].size = 0;
		f.nodes[i].start_block = -1;
		f.nodes[i].is_dir = 0;
	}
}

void blocks_init()
{
	printf("----------blocks init\n");
	for (int i = 0; i < f.nblocks; i++) {
		f.blocks[i].status = unusedblock;
		f.blocks[i].next_block = -1;
	}
}

int print_info()
{
	int j, start, k=0, h=0;
	for(int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used ) {
			printf("\n\n------File: %s---------\n", f.nodes[i].path);
			start = f.nodes[i].start_block;
			j = 0;
			while(start!=-1) {
				printf("blockindex: %d status: %d content: %s\n", start,f.blocks[start].status, f.blocks[start].data);
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
	for(int i = 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == usedblock ) {
			printf("%d, ", i);
			j+=1;
		}
		else if ( f.blocks[i].status == lastblock ) {
			printf("Last%d, ", i);
			j+=1;
		}
	}
	printf("\n------Total Blocks: %d----------\n\n",j);
	

	return 0;
}

node* get_free_node()
{
	for (int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == unused ){
			f.nodes[i].status = used;
			return &f.nodes[i];
		}
	}
	perror("No more nodes left!!");
	exit(0);
}

int get_free_block_index()
{
	for (int i = 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == unusedblock ){
			f.blocks[i].status = usedblock;
			return i;
		}
	}
	perror("No more blocks left!!");
	exit(0);
}

void* hello_init(struct fuse_conn_info *conn) 
{
	printf("--------------init\n");
	f.block_size = BLOCK_SIZE;
	f.nnodes = NNODES;
	f.nblocks = NBLOCKS;
	
	f.nodes = (node *) malloc(f.nnodes*sizeof(node));
	f.blocks = (block *) malloc(f.nblocks*sizeof(block));
	
	printf("--------------ReadingFromDisk\n");
	fp = fopen(filename, "r");
	if (fp != NULL) {
		fread(f.nodes, sizeof(node), f.nnodes, fp);
		fread(f.blocks, sizeof(block), f.nblocks, fp);
	}
	else {
		fp = fopen(filename, "w+");
		nodes_init();
		blocks_init();
	}
	fclose(fp);
	return &f;
}

void hello_destroy(void *v)
{
	printf("--------------WritingToDisk\n");
	fp = fopen(filename, "w");
	if (fp != NULL) {
		fwrite(f.nodes, sizeof(node), f.nnodes, fp);
		fwrite(f.blocks, sizeof(block), f.nblocks, fp);
		fclose(fp);
	}
	else {
		perror("Couldn't open file in write mode");
	}
	
	printf("--------------destroy\n");
	free(f.nodes);
	free(f.blocks);
} 

static int hello_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} 
	
	for (int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			stbuf->st_mode = f.nodes[i].mode;
			stbuf->st_nlink = 1;
			stbuf->st_size = f.nodes[i].size;		
			return 0;
		}
	}
	
	return -ENOENT;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	for (int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used ) {
			filler(buf, f.nodes[i].path+1, NULL, 0);
		}
	}

	print_info();

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	for (int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			return 0;
		}
	}
	//if ((fi->flags & 3) != O_RDONLY)
	//	return -EACCES;
	return -ENOENT;
}

static int hello_create(const char *path, mode_t mode, struct fuse_file_info *fi) 
{
	//open file if it exists. if not the create and open file
	if ( hello_open(path, fi) == -ENOENT ) {
		//file doesn't exist
		node* file;
		file = get_free_node();
		file->mode = mode;
		strcpy(file->path, path);
	}
	
	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes read
	size_t len, offset2=0;
	(void) fi;
	
	int fileblock, size2;

	for(int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0 ) {
			len = f.nodes[i].size;
			size2 = f.block_size;
			fileblock = f.nodes[i].start_block;
			
			if (fileblock == -1)
				return 0;
			
			while ( offset2 < len ) {	
				if ( offset2 + size2 > len ){
					size2 = len - offset2;
				}
				
				memcpy(buf + offset2, &f.blocks[fileblock].data , size2);
				fileblock = f.blocks[fileblock].next_block;
				offset2 += size2;
			}			
			return offset2;
		}
	}
	return -ENOENT;
}

static int hello_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes written
	size_t offset2 = 0;
	int fileblock, last, size2;
	
	for(int i=0; i<f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0 ) {
			size2 = f.block_size;
			fileblock = f.nodes[i].start_block;

			if (fileblock == -1) {
				fileblock = get_free_block_index();
				f.nodes[i].start_block = fileblock;
			}

			while ( offset2 < size ) {	
				if ( offset2 + size2 > size ) {
					size2 = size - offset2;
				}
				
				if ( fileblock == -1 ) {
					fileblock = get_free_block_index();
					f.blocks[last].next_block = fileblock;
				}

				memcpy(&f.blocks[fileblock].data, buf + offset2 , size2);
				
				last = fileblock;
				fileblock = f.blocks[fileblock].next_block;
				offset2 += size2;
			}

			f.blocks[last].status = lastblock;
			f.nodes[i].size = offset2;
			
			return offset2;	 
		}
	}
	return -ENOENT;
}

static int hello_mkdir(const char *path, mode_t mode)
{
	return 0;
}
static int hello_unlink(const char *path)
{
	return 0;
}
static int hello_rmdir(const char *path)
{
	return 0;
}
static int hello_rename(const char *path, const char *newpath)
{
	for(int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0 ) {
			strcpy(f.nodes[i].path, newpath);
			return 0;
		}
	}
	return -ENOENT;
}

static int free_block(int index)
{
	int next;
	f.blocks[index].status = unusedblock;
	next = f.blocks[index].next_block;
	f.blocks[index].next_block = -1;
	return next;
}

static int hello_truncate(const char *path, off_t offset)
{
	// truncates to zero
	int start;
	for(int i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0 ) {
			start = f.nodes[i].start_block;
			f.nodes[i].start_block = -1;
			while ( start != -1 ) {
				start = free_block(start);
			}
			f.nodes[i].size = 0;
			return 0;	
		}
	}
	return -ENOENT;
}
static int hello_opendir(const char *path, struct fuse_file_info *f)
{
	return 0;
}
static int hello_flush(const char *path, struct fuse_file_info *f)
{
	return 0;
}


static struct fuse_operations hello_oper = {
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
	.init 		= hello_init,
	.destroy 	= hello_destroy,
	.rename 	= hello_rename
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
