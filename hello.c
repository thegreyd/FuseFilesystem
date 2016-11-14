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

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

typedef struct 
{
	char data[block_size];
	int next_block;
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

	file = get_free_node();
	
	file->mode = S_IFREG | 0666;
	file->path = path;
	file->is_dir = 0;
	
	fileblock = get_free_block();
	memcpy(fileblock, content, strlen(content));
	file->size = strlen(content);
	file->start = fileblock;

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
	makefile("/file.txt","file");
	makefile("/b.txt","b");

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
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		for(int i=0; i<100; i++) {
			if ( node_bitmap[i]==1 && strcmp(path, nodes[i].path) == 0) {
				stbuf->st_mode = nodes[i].mode;
				stbuf->st_nlink = 1;
				stbuf->st_size = nodes[i].size;		
				return 0;
			}
		}
		res = -ENOENT;
	}

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	//if (strcmp(path, "/") != 0)
	//	return -ENOENT;

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
	size_t len;
	(void) fi;
	//if(strcmp(path, hello_path) != 0)
	//	return -ENOENT;

	for(int i=0; i<100; i++) {
		if ( node_bitmap[i]==1 && strcmp(path, nodes[i].path) == 0 ) {
			len = nodes[i].size;
			if (offset < len) {
				if (offset + size > len) {
					size = len - offset;
				}
				memcpy(buf, nodes[i].start + offset, size);
			} else{
				size = 0;			
			}
		}
	}

	return size;
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
