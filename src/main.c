#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define VERSION 1.0

int main(int ac, char** av) {
    // Help message
    if (ac != 4) {
        printf("usage: %s [host] [port] [nick/user]\nircdk %.1f\nby stx3plus1.\n", av[0], VERSION);
        return 0;
    }

    // Idk stuff
    char* host = av[1];
    char in_buffer[512];
    char out_buffer[512];
    struct hostent* hostent;
    struct sockaddr_in sockaddr_in;

    // Connect socket to IRC server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    hostent = gethostbyname(host);
    in_addr_t in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(atoi(av[2]));
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in))) return 3;

    // Initiate our IRC connection
    memset(out_buffer, 0, sizeof(out_buffer));
    sprintf(out_buffer, "NICK %s\r\nUSER %s 0 * : %s\r\n", av[3], av[3], av[3]);
    send(sockfd, out_buffer, strlen(out_buffer), 0);

    // shared memory.
    int* joined_channel;
    int shm = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    joined_channel = (int*)shmat(shm, NULL, 0);

    // Start handling the now active IRC connection
    pid_t p = fork();
    if (p == 0) {
        // child - handles printing messages
        int i;
        while ((i = read(sockfd, in_buffer, sizeof(in_buffer))) > 0) {
            // message sending without a prefix
            if (strstr(in_buffer, "End of /NAMES list.")) *joined_channel = 1;
            // auto pong when server sends a PING, also hides PING request
            if (strstr(in_buffer, "PING")) {
                char* tok = strtok(in_buffer, " ");
                tok = strtok(NULL, " ");
                char* pong = malloc(sizeof(in_buffer));
                sprintf(pong, "PONG %s\r\n", tok);
                send(sockfd, pong, strlen(pong), 0);
                free(pong);
            } else {
                write(1, in_buffer, i);
            }
            memset(in_buffer, 0, sizeof(in_buffer));
        }
    } else {
        // parent - handles sending messages
        while (1) {
            memset(out_buffer, 0, sizeof(out_buffer));
            fgets(out_buffer, sizeof(out_buffer), stdin);
            if (out_buffer[0] == '/') {
                out_buffer[0] = ' ';
            } else {
                if (joined_channel) {
                    char tmp_out[512];
                    memcpy(tmp_out, out_buffer, sizeof(out_buffer));
                    snprintf(out_buffer, sizeof(out_buffer), "PRIVMSG %s :%s", "#general", tmp_out);
                } else {
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
    shmdt(joined_channel);
    shmctl(shm, IPC_RMID, NULL);
    printf("IRC connection closed, exiting\n");
    return 0;
}