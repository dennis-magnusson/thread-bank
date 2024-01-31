#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

// Adds a new item to the end of the queue
void enqueue(queue_t* q, int* client_socket) {
  item_t* new_item = malloc(sizeof(item_t));
  if (new_item == NULL) {
    // Handle memory allocation failure
    perror("Failed to allocate memory for new queue item");
    return;
  }

  new_item->client_socket = client_socket;
  new_item->next = NULL;

  if (q->tail == NULL) {
    q->head = new_item;
  }
  else {
    q->tail->next = new_item;
  }
  q->tail = new_item;
  q->size++;
}


// Gets the next item from the queue. If queue is empty reuturn NULL
int* dequeue(queue_t* q) {
  if (q->head == NULL) {
    return NULL; // Queue is empty
  }

  int* result = q->head->client_socket;
  item_t* temp = q->head;
  q->head = q->head->next;
  if (q->head == NULL) {
    q->tail = NULL;
  }
  free(temp);
  q->size--;
  return result;
}
