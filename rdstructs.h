#define BLOCK_SIZE 4
#define PATH_MAX 1024
#define NBLOCKS 200
#define NNODES 200

typedef enum {usedblock, unusedblock} block_status;
typedef enum {used, unused} node_status;

typedef struct
{
	block_status status;
	char data[BLOCK_SIZE];
	int next_block;
} block;

typedef struct 
{
	node_status status;
	char path[PATH_MAX];
	int is_dir;
	mode_t mode;
	size_t size;
	int start_block;
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