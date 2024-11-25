#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define VERSION 1.0
#define buf_size 512

typedef struct {
    int alive, joined_channel;
    char channel[51];
} config;

int main(int ac, char** av) {
    // Help message
    if (ac != 4) {
        printf("usage: %s [host] [port] [nick/user]\nircdk %.1f\nby stx3plus1.\n", av[0], VERSION);
        return 0;
    }

    // Idk stuff
    char* host = av[1];
    char in_buffer[buf_size];
    char out_buffer[buf_size + 64];
    struct hostent* hostent;
    struct winsize ws;
    struct sockaddr_in sockaddr_in;

    // Get terminal size for TUI
    ioctl(0, TIOCGWINSZ, &ws);
    int w = ws.ws_col;
    int h = ws.ws_row;

    char* display_buffer[h - 1];
    display_buffer[h - 2] = "Welcome to ircdk! Connecting to server...\n";
    printf("%s", display_buffer[h - 2]);

    // Connect socket to IRC server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    hostent = gethostbyname(host);
    in_addr_t in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(atoi(av[2]));
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in))) {
        perror("connect()");
        return 1;
    }

    // Initiate our IRC connection
    memset(out_buffer, 0, sizeof(out_buffer));
    sprintf(out_buffer, "NICK %s\r\nUSER %s 0 * : %s\r\n", av[3], av[3], av[3]);
    send(sockfd, out_buffer, strlen(out_buffer), 0);

    // shared memory.
    config* conf;
    int shm = shmget(IPC_PRIVATE, sizeof(config), IPC_CREAT | 0666);
    conf = (config*)shmat(shm, NULL, 0);
    conf->alive = 1;
    conf->joined_channel = 0;

    // Start handling the now active IRC connection
    pid_t p = fork();
    if (p == 0) {
        // child - handles printing messages
        int i;
        while ((i = read(sockfd, in_buffer, sizeof(in_buffer))) > 0) {
            // message sending without a prefix
            if (strstr(in_buffer, "JOIN :")) {
                conf->joined_channel = 1;
                char* tok = strtok(in_buffer, "JOIN :");
                tok = strtok(NULL, "JOIN :");
                strncpy(conf->channel, tok, strlen(tok) - 1);
            }
            // auto pong when server sends a PING, also hides PING request
            if (strstr(in_buffer, "PING")) {
                char* tok = strtok(in_buffer, " ");
                tok = strtok(NULL, " ");
                char* pong = malloc(sizeof(in_buffer));
                sprintf(pong, "PONG %s\r\n", tok);
                send(sockfd, pong, strlen(pong), 0);
                free(pong);
            } else {
                if (write(1, in_buffer, i));
            }
            memset(in_buffer, 0, sizeof(in_buffer));
        }
        conf->alive = 0;
        printf("IRC connection closed. Press enter to exit.\n");
    } else {
        // parent - handles sending messages
        while (1) {
            memset(out_buffer, 0, sizeof(out_buffer));
            if (fgets(out_buffer, sizeof(out_buffer), stdin) == NULL) perror("fgets()");
            if (out_buffer[0] == '/') {
                out_buffer[0] = ' ';
            } else {
                if (conf->joined_channel) {
                    char tmp_out[buf_size];
                    memcpy(tmp_out, out_buffer, sizeof(tmp_out));
                    snprintf(out_buffer, sizeof(out_buffer), "PRIVMSG %s :%s", conf->channel, tmp_out);
                } else {
                    if (conf->alive == 0) break;
                    printf("Join a channel to chat. To run commands, prefix / to the command.\n");
                    continue;
                }
            }
            char end[3] = "\r\n\0";
            strncat(out_buffer, end, 3);
            send(sockfd, out_buffer, strlen(out_buffer), 0);
        }
    }
    if (sockfd) close(sockfd);
    shmdt(conf);
    shmctl(shm, IPC_RMID, NULL);
    return 0;
}