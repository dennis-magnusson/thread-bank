#ifndef QUEUE_H
#define QUEUE_H

struct item {
  struct item* next;
  int* client_socket;
};
typedef struct item item_t;

struct queue {
  item_t* head;
  item_t* tail;
  int size;
};
typedef struct queue queue_t;

void enqueue(queue_t* q, int* client_socket);
int* dequeue(queue_t* q);

#endif
