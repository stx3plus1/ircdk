/*
 * scrlbuf
 * by stx3plus1 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef buf_size
#define buf_size 512
#endif

typedef struct {
    char** buffer;
    int size, len, head;
} scrollbuffer;

void buffer_init(scrollbuffer* scrlbuf, int term_w, int term_h);
void buffer_destroy(scrollbuffer* scrlbuf);
void buffer_add(scrollbuffer* scrlbuf, char* line);
void buffer_show(scrollbuffer* scrlbuf, char* user, char* msg, int i);