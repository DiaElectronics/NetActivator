#ifndef DIAEQUEUE_H
#define DIAEQUEUE_H


#define DIAE_QUEUE_WAIT 1
#define DIAE_QUEUE_NOWAIT 0

#include <pthread.h>

typedef struct diae_queue_block
{
	void *data;
	struct diae_queue_block *next;
	struct diae_queue_block *last;
} diae_queue_block;

typedef struct diae_queue
{
    char * name;
    pthread_mutex_t lock;
	diae_queue_block *start_block;
	diae_queue_block *end_block;
} diae_queue;

int delete_from_queue(diae_queue * targetQueue, diae_queue_block * block_to_delete);
diae_queue * create_queue(char * newName);
diae_queue_block * enqueue(diae_queue * targetQueue, void * newData);
void * dequeue(diae_queue * targetQueue);
void lock_queue(diae_queue * tartgetQueue);
void unlock_queue(diae_queue * tartgetQueue);
int is_diae_queue_empty(diae_queue * targetQueue);
int get_elements_count(diae_queue * targetQueue);
void check_queue(diae_queue * targetQueue, char needDebug);

#endif // DIAEQUEUE_H
