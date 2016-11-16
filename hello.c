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

filesystem f;
FILE *fp;
char *filename = "ramdisk.save";

int print_contents()
{
	printf("---Contents of file system---\n");
	for(int i = 0; i < f.nnodes; i++) {
		if ( f.node_bitmap[i] == 1 ) {
			printf("%s\n", f.nodes[i].path);
		}
	}
	return 0;
}

int get_free_bit(int *bitmap, int size)
{
	for (int i = 0; i < size; i++) {
		if ( bitmap[i] == 0 )
			return i;
	}
	return -1;
}

node* get_free_node()
{
	int i = get_free_bit(f.node_bitmap, f.nnodes);
	f.node_bitmap[i] = 1;
	return &f.nodes[i];
}

block* get_free_block()
{
	int i = get_free_bit(f.block_bitmap, f.nblocks);
	f.block_bitmap[i] = 1;
	return &f.blocks[i];	
}

int makefile(char *path, char *content)
{
	printf("--------------makefile\n");
	node* file;
	block* fileblock;
	block* fileblock2;

	file = get_free_node();
	
	file->mode = S_IFREG | 0666;
	strcpy(file->path, path);
	file->is_dir = 0;
	
	int offset = 0;
	size_t len = strlen(content);

	fileblock = get_free_block();
	file->start = fileblock;
	
	int size = f.block_size;
	if (size > len){
		size = len - offset;
	}
	memcpy(fileblock, content, size);
	offset += size;
	
	while ( offset < len ) {
		if (offset + size > len){
			size = len - offset;
		}
	
		fileblock2 = get_free_block(f);
		fileblock->next_block = fileblock2;
		memcpy(fileblock2, content+offset, size);
		offset += size;
		fileblock = fileblock2;
	}

	fileblock->next_block = NULL;
	file->size = offset;

	return 0;
}

int makedir(char *path){
	printf("--------------makedir\n");

	node* file = get_free_node();
	
	file->mode = S_IFDIR | 0755;
	strcpy(file->path, path);
	file->is_dir = 1;
	file->start = NULL;

	return 0;
}

void* hello_init(struct fuse_conn_info *conn) 
{
	printf("--------------init\n");
	
	f.block_size = BLOCK_SIZE;
	f.nblocks = NBLOCKS;
	f.nnodes = NNODES;

	f.nodes = (node *) malloc(f.nnodes*sizeof(node));
	f.blocks = (block *) malloc(f.nblocks*sizeof(block));
	f.node_bitmap = (int *) malloc(f.nnodes*sizeof(int));
	f.block_bitmap = (int *) malloc(f.nblocks*sizeof(int));

	printf("--------------ReadingFromDisk\n");
	fp = fopen(filename, "r");
	if (fp != NULL) {
		fread(f.nodes, sizeof(node), f.nnodes, fp);
		fread(f.blocks, sizeof(block), f.nblocks, fp);
		fread(f.node_bitmap, sizeof(int), f.nnodes, fp);
		fread(f.block_bitmap, sizeof(int), f.nblocks, fp);
	}
	else {
		fp = fopen(filename, "w+");
	}
	fclose(fp);
	//makefile("/file.txt","filehellhellomypeople good boy ! poop");
	//makefile("/b.txt","bahah12323 &8&* !");

	return &f;
}

void hello_destroy(void *v)
{
	printf("--------------WritingToDisk\n");
	fp = fopen(filename, "w");
	fwrite(f.nodes, sizeof(node), f.nnodes, fp);
	fwrite(f.blocks, sizeof(block), f.nblocks, fp);
	fwrite(f.node_bitmap, sizeof(int), f.nnodes, fp);
	fwrite(f.block_bitmap, sizeof(int), f.nblocks, fp);
	fclose(fp);

	printf("--------------destroy\n");
	free(f.nodes);
	free(f.blocks);
	free(f.node_bitmap);
	free(f.block_bitmap);

} 

static int hello_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} 
	
	for(int i = 0; i < f.nnodes; i++) {
		if ( f.node_bitmap[i] == 1 && strcmp(path, f.nodes[i].path) == 0) {
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

	for(int i=0; i<f.nnodes; i++) {
		if ( f.node_bitmap[i]==1 ) {
			filler(buf, f.nodes[i].path+1, NULL, 0);
		}
	}

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	//if (strcmp(path, hello_path) != 0)
	//	return -ENOENT;
	for(int i=0; i<f.nnodes; i++) {
		if ( f.node_bitmap[i]==1 && strcmp(path, f.nodes[i].path) == 0 ) {
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
		file->is_dir = 0;
		file->size = 0;
		file->start = NULL;
	}
	
	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes read
	size_t len, offset2=0;
	(void) fi;
	
	block* fileblock;
	int size2;

	for(int i=0; i<f.nnodes; i++) {
		if ( f.node_bitmap[i]==1 && strcmp(path, f.nodes[i].path) == 0 ) {
			len = f.nodes[i].size;
			size2 = f.block_size;
			fileblock = f.nodes[i].start;

			while ( offset2 < len ) {	
				if ( offset2 + f.block_size > len ){
					size2 = len - offset2;
				}
				
				memcpy(buf + offset2, fileblock , size2);
				fileblock = fileblock->next_block;
				offset2+=size2;
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
	return 0;

}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
	.create		= hello_create,
	.write 		= hello_write,
	.init 		= hello_init,
	.destroy 	= hello_destroy
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
