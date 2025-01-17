/*
 * scrlbuf
 * by stx3plus1 
 */

#include "include/scrlbuf.h"

void buffer_init(scrollbuffer* scrlbuf, int term_w, int term_h) {
    scrlbuf->size = term_h - 1;
    scrlbuf->len = term_w;
    scrlbuf->head = 0;
    scrlbuf->buffer = malloc(sizeof(char*) * scrlbuf->size);
    for (int i = 0; i < scrlbuf->size; i++) {
        scrlbuf->buffer[i] = malloc(sizeof(char) * term_w);
        memset(scrlbuf->buffer[i], ' ', term_w);
    }
}

void buffer_destroy(scrollbuffer* scrlbuf) {
    for (int i = 0; i < scrlbuf->size; i++) {
        free(scrlbuf->buffer[i]);
    }
    free(scrlbuf->buffer);
}

void buffer_add(scrollbuffer* scrlbuf, char* line) {
    char* start = line;
    char buffer[buf_size];

    while (*start != '\0') {
        int i = 0;
        while (*start != '\n' && *start != '\0' && i < buf_size - 1) buffer[i++] = *start++;
        buffer[i] = '\0';
        if (buffer[0] != '\0') {
            strncpy(scrlbuf->buffer[scrlbuf->head], buffer, scrlbuf->len);
            scrlbuf->head = (scrlbuf->head + 1) % scrlbuf->size;
        }
        if (*start == '\n') start++;
    }
}

void buffer_show(scrollbuffer* scrlbuf, char* user, char* msg, int i) {
    printf("\ec");
    fflush(stdout);
    int start = scrlbuf->head;
    for (int i = 0; i < scrlbuf->size; i++) {
        int index = (start + i) % scrlbuf->size;
        if (scrlbuf->buffer[index][0] != '\0') printf("%s\n", scrlbuf->buffer[index]);
    }
    printf("[%s] ", user);
    fwrite(msg, i, 1, stdout);
    fflush(stdout);
}
