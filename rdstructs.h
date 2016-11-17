#define BLOCK_SIZE 4
#define PATH_MAX 1024
#define NBLOCKS 200
#define NNODES 200

typedef enum {usedblock, lastblock, unusedblock} block_status;
typedef enum {used, unused} node_status;

typedef struct
{
	block_status status;// = unusedblock;
	char data[BLOCK_SIZE];
	int next_block;// = -1;
} block;

typedef struct 
{
	node_status status;// = unused;
	char path[PATH_MAX];
	int is_dir;// = 0;
	mode_t mode;
	size_t size;// = 0;
	int start_block;// = -1;
} node;

typedef struct 
{
	size_t block_size;// = BLOCK_SIZE;
	size_t nblocks;// = NBLOCKS;
	size_t nnodes;// = NNODES;
	node* nodes;
	block* blocks;
	int* block_bitmap;
	int* node_bitmap;
} filesystem;