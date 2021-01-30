#ifndef __QUEUE__H
#define __QUEUE__H

struct queue;

struct queue* queue_new(void);
void* queue_front(struct queue*);
int queue_add(struct queue*, void*);
void* queue_remove(struct queue*);
