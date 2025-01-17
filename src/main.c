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

#include "include/scrlbuf.h"

#define VERSION "1.01"

#ifndef buf_size
#define buf_size 512
#endif

int irc_connect(char* host, int port) {
    // Structs needed
    struct hostent* hostent;
    struct sockaddr_in sockaddr_in;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    hostent = gethostbyname(host);
    in_addr_t in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in))) {
        perror("connect");
        // I really hope that your descriptor isn't conflicting with stdin
        return 0;
    } else {
        return sockfd;
    }
}

int main(int ac, char** av) {
    // Help message
    if (ac != 4) {
        printf("usage: %s [host] [port] [nick/user]\nircdk %s\nby stx3plus1.\n", av[0], VERSION);
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
    char out_buffer[buf_size + 64]; // out needs to be larger, i dont remmeber why but its to do with the message sending

    // information.
    int alive = 1;
    int joined_channel = 0;
    char channel[51];

    // Create message buffer
    scrollbuffer display_buffer;
    buffer_init(&display_buffer, w, h);

    // Remove terminal echo
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    // The socket should not be blocking either.
    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	buffer_add(&display_buffer, "starting IRC connection...\none moment please.");

    // Initiate our IRC connection
    memset(out_buffer, 0, sizeof(out_buffer));
    sprintf(out_buffer, "NICK %s\r\nUSER %s 0 * : %s\r\n", av[3], av[3], av[3]);
    send(sockfd, out_buffer, strlen(out_buffer), 0);

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
					if (strstr(out_buffer, "/clear")) {
						for (int i = 0; i < h; i++) buffer_add(&display_buffer, " ");
						buffer_show(&display_buffer, av[3], " ", 0);
						continue;
					}
                    out_buffer[0] = ' ';
                } else if (out_buffer[0] == '\0') {
                    buffer_add(&display_buffer, "Type text to send a message.");
                    buffer_show(&display_buffer, av[3], out_buffer, i);
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

                        buffer_show(&display_buffer, av[3], "", 0);
                    } else {
                        if (alive) buffer_add(&display_buffer, "Join a channel to chat. To run commands, prefix / to the command.\n");
                        buffer_show(&display_buffer, av[3], out_buffer, 0);
                        continue;
                    }
                }
                // Send the packet, if it is command or message.
                strncat(out_buffer, "\r\n\0", 3);
                send(sockfd, out_buffer, strlen(out_buffer), 0);
                memset(out_buffer, 0, sizeof(out_buffer));
            } else if (c == '\b' || c == 127) {
                if (i > 0) {
                    i--;
                    printf("\b \b");
                    fflush(stdout);
                }
                buffer_show(&display_buffer, av[3], out_buffer, i);
            } else if (c == 27) {
                printf("\n%s left\n", av[3]);
                alive = 0;
                break;
            }
            else {
                out_buffer[i++] = c;
                buffer_show(&display_buffer, av[3], out_buffer, i);
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
            if (strstr(in_buffer, "PRIVMSG")) {
                // Format messages from other users
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
            } else if (strstr(in_buffer, ":Closing link")) {
                printf("%s left\n", av[3]);
                alive = 0;
                break;
            } else if (strstr(in_buffer, "QUIT")) {
                char* tok = strtok(in_buffer, "!");
                *tok++;
                char* left = malloc(sizeof(in_buffer));
                sprintf(left, "%s left", tok);
                buffer_add(&display_buffer, left);
                free(left);
            } else if (strstr(in_buffer, "JOIN")) { 
                char* tok = strtok(in_buffer, "!");
                *tok++;
                char* join = malloc(sizeof(in_buffer));
                sprintf(join, "%s joined", tok);
                buffer_add(&display_buffer, join);
                free(join);
            } else {
                buffer_add(&display_buffer, in_buffer);
            }
            memset(in_buffer, 0, sizeof(in_buffer));
           	buffer_show(&display_buffer, av[3], out_buffer, i);
        }
    }
    if (sockfd) close(sockfd);
    buffer_destroy(&display_buffer);

    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
    return 0;
}
