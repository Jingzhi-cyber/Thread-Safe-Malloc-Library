#include "my_malloc.h"

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

// Initialize our list with head and tail pointers to NULL
__thread freedNode * head = NULL;
__thread freedNode * tail = NULL;

unsigned long data_segment_size = 0;

//mutex for thread safe malloc/free with lock
pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;

//mutex for sbrk()
pthread_mutex_t sbrk_Mutex = PTHREAD_MUTEX_INITIALIZER;

void * my_malloc(size_t size, int ff_bf) {
  if (head == NULL) {
    return getMoreSpace(size);
  }

  freedNode * currNode = head;
  void * ans = NULL;
  if (ff_bf == 1) {  // ff
    while (currNode != NULL) {
      if (currNode->space >= size) {
        splitNode(currNode, size);
        ans = (uint8_t *)currNode + NODE_SIZE;
        removeNode(currNode);
        break;
      }

      currNode = currNode->next;
    }
  }
  if (ff_bf == 2) {  // bf
    freedNode * nodeToPick = NULL;
    int difference = INT32_MAX;
    while (currNode != NULL) {
      if (currNode->space >= size) {
        int new_difference = currNode->space - size;
        if (new_difference == 0) {
          nodeToPick = currNode;
          break;
        }
        if (new_difference < difference) {
          difference = new_difference;
          nodeToPick = currNode;
        }
      }
      currNode = currNode->next;
    }
    if (nodeToPick != NULL) {
      splitNode(nodeToPick, size);
      ans = (uint8_t *)nodeToPick + NODE_SIZE;
      removeNode(nodeToPick);
    }
  }
  // Can not find freed block in the list
  if (ans == NULL) {
    ans = getMoreSpace(size);
  }
  return ans;
}

void * ff_malloc(size_t size) {
  return my_malloc(size, 1);
}

void ff_free(void * ptr) {
  if (ptr == NULL) {
    return;
  }

  freedNode * currNode = (freedNode *)((uint8_t *)ptr - NODE_SIZE);

  addNode(currNode);
  tryMerge(currNode);
}

void bf_free(void * ptr) {
  ff_free(ptr);
}

void * bf_malloc(size_t size) {
  return my_malloc(size, 2);
}

void * getMoreSpace(size_t space) {
  pthread_mutex_lock(&sbrk_Mutex);
  void * prev_prog_break = sbrk(space + NODE_SIZE);
  pthread_mutex_unlock(&sbrk_Mutex);

  if (prev_prog_break == (void *)-1) {
    return NULL;  // malloc error
  }

  data_segment_size += (space + NODE_SIZE);

  freedNode * currNode = prev_prog_break;

  currNode->space = space;
  currNode->prev = NULL;
  currNode->next = NULL;

  return (uint8_t *)prev_prog_break + NODE_SIZE;
}

void removeNode(freedNode * currNode) {
  if (currNode == head && currNode == tail) {
    head = NULL;
    tail = NULL;
    return;
  }

  if (currNode == head && currNode != tail) {
    head = currNode->next;
    currNode->next->prev = NULL;
    return;
  }
  if (currNode != head && currNode == tail) {
    tail = currNode->prev;
    currNode->prev->next = NULL;
    return;
  }
  currNode->next->prev = currNode->prev;
  currNode->prev->next = currNode->next;
}

void splitNode(freedNode * currNode, size_t space) {
  size_t space_left = currNode->space - space;
  if (space_left <= NODE_SIZE) {
    // can not split
    return;
  }

  freedNode * secondNode = (freedNode *)((uint8_t *)currNode + NODE_SIZE + space);
  secondNode->space = space_left - NODE_SIZE;
  secondNode->prev = NULL;
  secondNode->next = NULL;
  currNode->space = space;
  addNode(secondNode);
}

void addNode(freedNode * toAdd) {
  if (head == NULL) {
    head = toAdd;
    tail = toAdd;
    head->next = NULL;
    head->prev = NULL;
    return;
  }

  if (head > toAdd) {
    toAdd->next = head;
    head->prev = toAdd;
    head = toAdd;
    toAdd->prev = NULL;
    return;
  }

  if (toAdd > tail) {
    toAdd->prev = tail;
    tail->next = toAdd;
    tail = toAdd;
    toAdd->next = NULL;
    return;
  }

  freedNode * currNode = head->next;
  while (currNode != NULL) {
    if (currNode > toAdd) {
      toAdd->prev = currNode->prev;
      toAdd->next = currNode;
      currNode->prev = toAdd;
      toAdd->prev->next = toAdd;
      break;
    }
    currNode = currNode->next;
  }
}

void tryMerge(freedNode * currNode) {
  if (!currNode->next && !currNode->prev) {
    return;
  }
  if (!currNode->next && currNode->prev) {
    mergeADJ(currNode->prev, currNode);
    return;
  }
  if (currNode->next && !currNode->prev) {
    mergeADJ(currNode, currNode->next);
    return;
  }
  freedNode * prev = currNode->prev;
  freedNode * next = currNode->next;
  freedNode * leftSide = mergeADJ(prev, currNode);
  if (leftSide == NULL) {
    mergeADJ(currNode, next);
  }
  else {
    mergeADJ(prev, next);
  }
}

freedNode * mergeADJ(freedNode * first, freedNode * second) {
  if ((uint8_t *)first + NODE_SIZE + first->space == (uint8_t *)second) {
    first->space = first->space + NODE_SIZE + second->space;
    removeNode(second);
    return first;
  }
  return NULL;
}

unsigned long get_data_segment_size() {
  return data_segment_size;
}

unsigned long get_data_segment_free_space_size() {
  unsigned long ans = 0;
  freedNode * curr = head;
  while (curr != NULL) {
    ans = ans + curr->space + NODE_SIZE;
    curr = curr->next;
  }
  return ans;
}

// Thread safe version of malloc based on Best-Fit using mutex
void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&myMutex);
  void * ans = bf_malloc(size);
  pthread_mutex_unlock(&myMutex);
  return ans;
}

// Thread safe version of free based on Best-Fit using mutex
void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&myMutex);
  bf_free(ptr);
  pthread_mutex_unlock(&myMutex);
}

//Thread Safe version of malloc based on Best-Fit without using mutex
void * ts_malloc_nolock(size_t size) {
  return bf_malloc(size);
}

//Thread Safe version of malloc based on Best-Fit without using mutex
void ts_free_nolock(void * ptr) {
  bf_free(ptr);
}

/*
int main() {
  int * a = ff_malloc(sizeof(int));
  int * b = ff_malloc(sizeof(int));

  *a = 4;

  *b = 8;

  printf("Block size: %lu \n", NODE_SIZE);

  printf("Address of a : %p \n", (void *)a);
  printf("Address of b : %p \n", (void *)b);

  ff_free(a);
  ff_free(b);

  int * c = ff_malloc(sizeof(int));

  printf("Address of c : %p \n", (void *)c);

  *c = 10;

  ff_free(c);

  int * d = ff_malloc(sizeof(int));

  *d = 8;

  return 0;
}
*/
