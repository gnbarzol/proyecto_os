#include "queue.h"

void create_queue(Queue *q){
    (*q) = NULL;
}

void destroy_queue(Queue *q){
    if((*q) == NULL) return;
    else {
        Queue aux = (*q);
        (*q) = (*q)->post;
        free(aux);
        destroy_queue(q);
    }
}

void enqueue(Queue *q, char *val){
    if((*q)==NULL){
        (*q) = (Queue)malloc(sizeof(struct TNode));
        (*q)->value = val;
        (*q)->post = NULL;
    } else {
        Queue aux = (*q);
        Queue prev = NULL;
        while((*q)!=NULL) {
            prev = (*q);
            (*q) = (*q)->post;
        }
        (*q) = (Queue)malloc(sizeof(struct TNode));
        (*q)->value = val;
        (*q)->post = NULL;
        prev->post = (*q);
        (*q) = aux;
    }
}

char * dequeue(Queue *q){
    if((*q)==NULL) return NULL;
    else {
        Queue aux = (*q);
        (*q) = (*q)->post;
        aux->post = NULL;
        return aux->value;
    }
}

char * lastqueue(Queue *q) {
    if((*q) == NULL) return NULL;
    else if(size_queue((*q)) == 1) {
        return dequeue(q);
    }
    else{
        Queue aux = (*q);
        Queue prevLast = NULL;
        Queue last = NULL;
        while((*q)!=NULL) {
            last = (*q);
            if((*q)->post != NULL) {
                prevLast = (*q);
            }
            (*q) = (*q)->post;
        }
        prevLast->post = NULL;
        (*q) = aux;
        return last->value;
    }
}

char * first_queue(Queue q){
    if(q==NULL) return NULL;
    return q->value;
}

int size_queue(Queue q){
    if (q == NULL) return 0;
    return 1+size_queue(q->post);
}

int isEmpty_queue(Queue q){
    if (q==NULL) return 1;
    return 0;
}

void show_queue(Queue q){
    if(q==NULL)  
        printf("NULL\n");
    else {
        printf("%s->",q->value);
        show_queue(q->post);
    }
   
}
