#include "diaequeue.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>


diae_queue * create_queue(char * newName)
{
    diae_queue * result;
    result = (diae_queue *) malloc (sizeof(diae_queue));
    result->end_block = NULL;
    result->start_block = NULL;
    result->name = newName;
    return result;
}

int get_elements_count(diae_queue * targetQueue)
{
    lock_queue(targetQueue);
    int res = 0;
    diae_queue_block * curBlock = targetQueue->start_block;
    while(curBlock!=NULL)
    {
        curBlock = curBlock->next;
        res++;
    }
    unlock_queue(targetQueue);
    return res;
}
diae_queue_block * enqueue(diae_queue * targetQueue, void * newData)
{
    if(targetQueue == NULL)
    {
        return NULL;
    }
    lock_queue(targetQueue);
    diae_queue_block * newBlock = (diae_queue_block *)malloc(sizeof(diae_queue_block));
    newBlock->data = newData;
    if(targetQueue->end_block == NULL)
    {
        targetQueue->start_block = newBlock;
        targetQueue->end_block = newBlock;
        newBlock->last = NULL;
        newBlock->next = NULL;
    }
    else
    {
        targetQueue->end_block->next = newBlock;
        newBlock->last=targetQueue->end_block;
        targetQueue->end_block = newBlock;
        targetQueue->end_block->next = NULL;
    }
    unlock_queue(targetQueue);
    return newBlock;
}
void lock_queue(diae_queue * targetQueue)
{
    if(targetQueue == NULL) return;
    //printf("lock start on %s ...", targetQueue->name);
    pthread_mutex_lock(&targetQueue->lock);
    //printf(" locked! \n");
}
void unlock_queue(diae_queue * targetQueue)
{
    if(targetQueue == NULL) return;
    //printf("unlock start on %s ...", targetQueue->name);
    pthread_mutex_unlock(&targetQueue->lock);
    //printf(" unlocked! \n");
}

void * dequeue(diae_queue * targetQueue)
{
    lock_queue(targetQueue);
    if(targetQueue == NULL)
    {
        pthread_mutex_unlock(&targetQueue->lock);
        return NULL;
    }
    else if(targetQueue->end_block == NULL)
    {
        pthread_mutex_unlock(&targetQueue->lock);
        return NULL;
    }
    else if(targetQueue->end_block == targetQueue->start_block)
    {
        void * result = targetQueue->end_block->data;
        free(targetQueue->start_block);
        targetQueue->start_block = NULL;
        targetQueue->end_block = NULL;
        unlock_queue(targetQueue);
        return result;
    }
    else
    {
        void * result = targetQueue->start_block->data;
        diae_queue_block * newStartBlock = targetQueue->start_block->next;
        newStartBlock->last = NULL;
        free(targetQueue->start_block);
        targetQueue->start_block=newStartBlock;
        unlock_queue(targetQueue);
        return result;
    }
    unlock_queue(targetQueue);
    return NULL;
}
void check_queue(diae_queue * targetQueue, char needDebug)
{
    if(needDebug) printf("checking queue...\n");
    lock_queue(targetQueue);
    if(targetQueue->start_block!=NULL)
    {
        if(targetQueue->start_block->last != NULL)
        {
            printf("ERR: start->prev is NOT NULL\n\n");
        }
    }
    if(needDebug) printf("1\n");
    if(targetQueue->end_block!=NULL)
    {
        if(targetQueue->end_block->next != NULL)
        {
            printf("ERR: end->next is NOT NULL\n\n");
        }
    }
    if(needDebug) printf("2\n");
    if(targetQueue->start_block!=NULL && targetQueue->end_block == NULL)
    {
        printf("ERR: queue destroyed");
    }
    if(needDebug) printf("3\n");
    if(targetQueue->start_block==NULL && targetQueue->end_block != NULL)
    {
        printf("ERR: queue completely destroyed");
    }

    if(needDebug) printf("4\n");
    diae_queue_block * curBlock = targetQueue->start_block;

    if(needDebug) printf("5\n");
    while(curBlock!=NULL)
    {
        if(needDebug) printf("6\n");
        diae_queue_block * prev = curBlock->last;
        if(prev!=NULL)
        {
            if(prev->next!=curBlock)
            {
                printf("ERR blocks connections are wrong\n\n");
            }
        }
        if(curBlock == curBlock->next)
        {
            printf("queue is cyclic\n");
        }
        curBlock=curBlock->next;
    }
    unlock_queue(targetQueue);


}


int is_diae_queue_empty(diae_queue * targetQueue)
{
    if(targetQueue == NULL)
    {
        printf("cant check NULL queue");
        return 1;
    }
    if(targetQueue->start_block == NULL && targetQueue->end_block!=NULL)
    {
        printf("queue is damaged...\n");
    }
    return targetQueue->start_block == NULL;
}

int delete_from_queue(diae_queue * targetQueue, diae_queue_block * block_to_delete)
{
    if(block_to_delete == NULL)
    {
        printf("attempt to delete an empty element\n");
        return 4;
    }
    if(targetQueue == NULL)
    {
        printf("attempt to delete from NULL queue");
        return 1;
    }
    if(targetQueue->start_block == NULL)
    {
        printf("attempt to delete from empty queue\n");
        return 2;
    }
    lock_queue(targetQueue);
    diae_queue_block * curBlock = targetQueue->start_block;
    while(curBlock!=NULL)
    {
        if(curBlock == block_to_delete)
        {
            diae_queue_block *prev = curBlock->last;
            diae_queue_block *next = curBlock->next;
            if(prev == NULL && next == NULL)
            {
                targetQueue->start_block = NULL;
                targetQueue->end_block = NULL;
                free(curBlock);
                unlock_queue(targetQueue);
                return 0;
            }
            if(prev == NULL)
            {
                //your block is the first one
                targetQueue->start_block = next;

                targetQueue->start_block->last=NULL;
                free(curBlock);
                if(targetQueue->start_block == NULL)
                {
                    targetQueue->end_block = NULL;
                }
                unlock_queue(targetQueue);
                return 0;
            }
            else
            {
                prev->next = next;
                free(curBlock);
                if(prev->next == NULL)
                {
                    targetQueue->end_block = prev;
                }
                unlock_queue(targetQueue);
                return 0;
            }

        }
        curBlock = curBlock->next;
    }
    unlock_queue(targetQueue);
    return 3;
}


