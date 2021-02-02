#ifndef QUEUE_H

#define QUEUE_H
#include <stdio.h>
#include <stdlib.h>

typedef struct TNode* Queue;
struct TNode{
    char *value;
    Queue post;
};

void create_queue(Queue *q);
void destroy_queue(Queue *q);
void enqueue(Queue *q, char *val);
char * dequeue(Queue *q);
char * lastqueue(Queue *q);
char *first_queue(Queue q);
int size_queue(Queue q);
int isEmpty_queue(Queue q);
void show_queue(Queue q);



#endif /* QUEUE_H */