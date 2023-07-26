#ifndef __MY_MALLOC_H__
#define __MY_MALLOC_H__

#include <stdio.h>

// build a doubly linked list to keep track of freed blocks in heap
// everytime ff_malloc or bf_malloc is called, there are two parts of space in heap is requested, the first is the space for store our node - metadata, the second is the space user claims
struct _freedNode {
  size_t space;
  struct _freedNode * next;
  struct _freedNode * prev;
};

typedef struct _freedNode freedNode;

#define NODE_SIZE sizeof(freedNode)

//THread safe versions of malloc/free are based on Best-Fit algorithm

//Thread Safe malloc/free: locking version
void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);

//Thread Safe malloc/free: non-locking version
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);

void * ff_malloc(size_t size);  //Fisrt fit
void ff_free(void * ptr);

void * bf_malloc(size_t size);  //Best fit
void bf_free(void * ptr);

void * my_malloc(
    size_t size,
    int ff_bf);  // if ff_bf = 1, then perform ff; if ff_bf = 2, then perform bf

// FF:: check the list from head, trying to find one node that node->space >= (request space + sizeof(node))
// if found , split the node into two, rm the fist nodefrom the list, then return the proper address
// if not found, call sbrk() to increase program break , then return the proper address
// To free, add the node into the list ,then check for "merging spaces"

// BF:: almost the same as FF, only need to change the method to check aviliable blocks in the freedList

// call sbrk() to get the address of the newly allocated space
void * getMoreSpace(size_t space);

void removeNode(freedNode * currNode);

void splitNode(freedNode * currNode, size_t space);

void addNode(freedNode * toAdd);

void tryMerge(freedNode * currNode);

freedNode * mergeADJ(freedNode * first, freedNode * second);

unsigned long get_data_segment_size();             //in bytes
unsigned long get_data_segment_free_space_size();  //in byte

#endif
