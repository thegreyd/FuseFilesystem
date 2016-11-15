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

#define block_size 4

typedef struct b
{
	char data[block_size];
	struct b* next_block;
} block;

typedef struct 
{
	char *path;
	int is_dir;
	int mode;
	int size;
	block* start;
} node;

/*static*/ int node_bitmap[100];
/*static*/ node* nodes;
block* blocks;
int block_bitmap[100];

int print_contents()
{
	printf("---Contents of file system---\n");
	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 ) {
			printf("%s\n", nodes[i].path);
		}
	}
	return 0;
}

int get_free_bit(int bitmap[])
{
	for (int i =0;i<100;i++) {
		if ( bitmap[i] == 0 )
			return i;
	}
	return -1;
}

node* get_free_node()
{
	int i = get_free_bit(node_bitmap);
	node_bitmap[i] = 1;
	return &nodes[i];
}

block* get_free_block()
{
	int i = get_free_bit(block_bitmap);
	block_bitmap[i] = 1;
	return &blocks[i];	
}

int makefile(char *path, char *content)
{
	printf("--------------makefile\n");
	node* file;
	block* fileblock;
	block* fileblock2;

	file = get_free_node();
	
	file->mode = S_IFREG | 0666;
	file->path = path;
	file->is_dir = 0;
	
	int offset = 0;
	size_t len = strlen(content);

	fileblock = get_free_block();
	file->start = fileblock;
	
	int size = block_size;
	if (size > len){
		size = len - offset;
	}
	memcpy(fileblock, content, size);
	offset += size;
	
	while ( offset < len ) {
		if (offset + size > len){
			size = len - offset;
		}
	
		fileblock2 = get_free_block();
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
	file->path = path;
	file->is_dir = 1;
	file->start = NULL;

	return 0;
}

int myinit(){
	printf("--------------init\n");
	
	nodes = (node *) malloc(100*sizeof(node));
	blocks = (block *) malloc(100*sizeof(block));
	makefile("/file.txt","filehellhellomypeople good boy ! poop");
	makefile("/b.txt","bahah12323 &8&* !");

	return 0;
}

int mydestroy(){
	printf("--------------mydestroy\n");
	free(nodes);
	return 0;
}

/*void* init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
	printf("--------fuse_init\n");
	myinit();
}


void destroy(void *p)
{
	printf("--------------fusedestroy\n");
	mydestroy();
	return destroy();
}
*/

static int hello_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} 
	
	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 && strcmp(path, nodes[i].path) == 0) {
			stbuf->st_mode = nodes[i].mode;
			stbuf->st_nlink = 1;
			stbuf->st_size = nodes[i].size;		
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

	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 ) {
			filler(buf, nodes[i].path+1, NULL, 0);
		}
	}

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	//if (strcmp(path, hello_path) != 0)
	//	return -ENOENT;
	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 && strcmp(path, nodes[i].path) == 0 ) {
			return 0;
		}
	}
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return -ENOENT;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes read
	size_t len, offset2=0;
	(void) fi;
	
	block* fileblock;
	int size2;

	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 && strcmp(path, nodes[i].path) == 0 ) {
			len = nodes[i].size;
			size2 = block_size;
			fileblock = nodes[i].start;

			while (offset2 < len) {	
				if (offset2 + block_size > len){
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


static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	myinit();
	return fuse_main(argc, argv, &hello_oper, NULL);
}
