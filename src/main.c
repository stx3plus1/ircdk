/*
 * ircdk 1.0
 * by stx3plus1
 */

#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define VERSION 1.0
#define buf_size 2048

typedef struct {
    char** buffer;
    int size, len, head;
} scrollbuffer;

void buffer_init(scrollbuffer* scrlbuf, int term_w, int term_h) {
    scrlbuf->size = term_h - 1;
    scrlbuf->len = term_w;
    scrlbuf->head = 0;
    scrlbuf->buffer = malloc(sizeof(char*) * scrlbuf->size);
    for (int i = 0; i < scrlbuf->size; i++) {
        scrlbuf->buffer[i] = malloc(sizeof(char) * term_w);
        memset(scrlbuf->buffer[i], 1, term_w);
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

void show_buffer(scrollbuffer* scrlbuf, char* user, char* msg) {
    printf("\ec");
    fflush(stdout);
    int start = scrlbuf->head;
    for (int i = 0; i < scrlbuf->size; i++) {
        int index = (start + i) % scrlbuf->size;
        if (scrlbuf->buffer[index][0] != '\0') printf("%s\n", scrlbuf->buffer[index]);
    }
    printf("[%s] %s", user, msg);
    if (*msg == '\0') printf("\e[sType your message...\e[u");
    fflush(stdout);
}

int irc_connect(char* host, int port) {
    struct hostent* hostent;
    struct sockaddr_in sockaddr_in;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    hostent = gethostbyname(host);
    in_addr_t in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in))) {
        perror("connect()");
        return 0;
    } else {
        return sockfd;
    }
}

int main(int ac, char** av) {
    // Help message
    if (ac != 4) {
        printf("usage: %s [host] [port] [nick/user]\nircdk %.1f\nby stx3plus1.\n", av[0], VERSION);
        return 0;
    }

    // Get terminal size for TUI
    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);
    int w = ws.ws_col;
    int h = ws.ws_row;

    // Connect socket to IRC server
    int sockfd = irc_connect(av[1], atoi(av[2]));
    if (!sockfd) {
        return 1;
    }

    // network buffers
    char in_buffer[buf_size];
    char out_buffer[buf_size + 64];
    // Initiate our IRC connection
    memset(out_buffer, 0, sizeof(out_buffer));
    sprintf(out_buffer, "NICK %s\r\nUSER %s 0 * : %s\r\n", av[3], av[3], av[3]);
    send(sockfd, out_buffer, strlen(out_buffer), 0);

    // information.
    int alive = 1;
    int joined_channel = 0;
    char channel[51];

    // Create message buffer
    scrollbuffer display_buffer;
    buffer_init(&display_buffer, w, h);

    // Remove all automatic terminal interaction.
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // The socket should not be blocking either.
    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Start handling the now active IRC connection
    int i = 0;
    char c = 0;
    memset(out_buffer, 0, sizeof(out_buffer));
    while (alive) {
         // handles sending messages
        if (read(0, &c, 1) > 0) {
            if (c == '\n') {
                putchar('\n');
                out_buffer[i++] = '\0';
                c = -1; i = 0;
                if (out_buffer[0] == '/') {
                    out_buffer[0] = ' ';
                } else if (out_buffer[0] == '\0') {
                    buffer_add(&display_buffer, "Type text to send a message.");
                    show_buffer(&display_buffer, av[3], out_buffer);
                    continue;
                } else {
                    if (joined_channel) {
                        char tmp_out[buf_size];
                        char msg_out[buf_size];
                        sprintf(msg_out, "[%s] %s", av[3], out_buffer);
                        buffer_add(&display_buffer, msg_out);

                        memcpy(tmp_out, out_buffer, sizeof(tmp_out));
                        snprintf(out_buffer, sizeof(out_buffer), "PRIVMSG %s :%s", channel, tmp_out);
                        memset(tmp_out, 0, buf_size);

                        show_buffer(&display_buffer, av[3], "");
                    } else {
                        if (alive) buffer_add(&display_buffer, "Join a channel to chat. To run commands, prefix / to the command.\n");
                        show_buffer(&display_buffer, av[3], out_buffer);
                        continue;
                    }
                }
                // Send the packet, if it is command or message.
                strncat(out_buffer, "\r\n\0", 3);
                send(sockfd, out_buffer, strlen(out_buffer), 0);
                memset(out_buffer, 0, sizeof(out_buffer));
            } else if (c == '\b' || c == 127) {
                if (i > 1) {
                    out_buffer[i--] = '\0';
                    printf("\b \b\e[2D");
                    fflush(stdout);
                }
                show_buffer(&display_buffer, av[3], out_buffer);
            }    
            else {
                out_buffer[i++] = c;
                show_buffer(&display_buffer, av[3], out_buffer);
            }
        }

        // handles printing messages
        if (read(sockfd, in_buffer, sizeof(in_buffer)) > 0) {
            char* dup = strdup(in_buffer);
            // message sending without a prefix
            if (strstr(in_buffer, "JOIN :")) {
                joined_channel = 1;
                char* tok = strtok(dup, "JOIN :");
                tok = strtok(NULL, "JOIN :");
                memset(channel, 0, sizeof(channel));
                strncpy(channel, tok, strlen(tok) - 1);
            }
            // Format messages from other users
            if (strstr(in_buffer, "PRIVMSG")) {
                char* message = strtok(in_buffer, ":");
                message = strtok(NULL, ":");
                char* tok = strtok(dup, "!");
                *tok++;
                char* msg_buf = malloc(sizeof(tok) + sizeof(in_buffer));
                sprintf(msg_buf, "[%s] %s", tok, message);
                buffer_add(&display_buffer, msg_buf);
                free(msg_buf);
            } else if (strstr(in_buffer, "PING")) {
                // auto pong when server sends a PING, also hides PING request
                char* tok = strtok(in_buffer, " ");
                tok = strtok(NULL, " ");
                char* pong = malloc(sizeof(in_buffer));
                sprintf(pong, "PONG %s\r\n", tok);
                send(sockfd, pong, strlen(pong), 0);
                free(pong);
            } else {
                buffer_add(&display_buffer, in_buffer);
            }
            memset(in_buffer, 0, sizeof(in_buffer));
            show_buffer(&display_buffer, av[3], out_buffer);
        }
    }
    if (sockfd) close(sockfd);
    buffer_destroy(&display_buffer);
    return 0;
}