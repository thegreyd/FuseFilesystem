#define BLOCK_SIZE 4
#define PATH_MAX 1024
#define NBLOCKS 200
#define NNODES 200

typedef struct b
{
	char data[BLOCK_SIZE];
	struct b* next_block;
} block;

typedef struct 
{
	char path[PATH_MAX];
	int is_dir;
	mode_t mode;
	size_t size;
	block* start;
} node;

typedef struct 
{
	size_t block_size;
	size_t nblocks;
	size_t nnodes;
	node* nodes;
	block* blocks;
	int* block_bitmap;
	int* node_bitmap;
} filesystem;