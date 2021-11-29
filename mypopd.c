#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

#define USER 1253
#define PASS 1271
#define STAT 1248
#define LIST 1298
#define RETR 1283
#define DELE 1138
#define RSET 1264
#define NOOP 1270
#define QUIT 1289

#define HELP 758 // update hash

typedef enum {
    GREETING_STATE,
    AUTHORIZATION_STATE,
    TRANSACTION_STATE,
    UPDATE_STATE,
    QUIT_STATE
} state_t;

state_t state;

static void handle_client(int fd);

int hash_command(char *command);

void clear();

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    char recvbuf[MAX_LINE_LENGTH + 1];
    net_buffer_t nb = nb_create(fd, MAX_LINE_LENGTH);

    state = GREETING_STATE;
    send_formatted(fd, "+OK POP3 server ready\n");
    state = AUTHORIZATION_STATE;


    while (1) {
        int len = nb_read_line(nb, recvbuf);

        // Check client input validity and length
        if (len == 0 || len == -1) break;
        char *line = strtok(recvbuf, "\r\n");
        if (strlen(line) == MAX_LINE_LENGTH) {
            send_formatted(fd, "500 Line is too long\n");
            continue;
        }

        char *command = strtok(line, " ");
        int hashed_command = hash_command(command);
        printf("%d\n", hashed_command);
        if  (hashed_command == HELP) {
            send_formatted(fd, "502 Unsupported command: %s\n", command);
            continue;
        }

        switch (hashed_command) {
            case USER:
                send_formatted(fd, "+OK\n");
                break;
            case PASS:
                send_formatted(fd, "+OK\n");
                break;
            case STAT:
                send_formatted(fd, "+OK\n");
                break;
            case LIST:
                send_formatted(fd, "+OK\n");
                break;
            case RETR:
                send_formatted(fd, "+OK\n");
                break;
            case DELE:
                send_formatted(fd, "+OK\n");
                break;
            case RSET:
                clear();
                send_formatted(fd, "+OK\n");
                break;
            case NOOP:
                send_formatted(fd, "+OK\n");
                break;
            case QUIT:
                send_formatted(fd, "+OK dewey POP3 server signing off\n");
                goto quit;
            default:
                send_formatted(fd, "-ERR invalid command: %s\n", command);
        }

    }

    quit: ;
    clear();
    nb_destroy(nb);
}

// undelete messages
void clear() {

}

int hash_command(char *command) {
    if ( strlen(command) > 4 ) return -1;
    return ((int) command[0]) + 3*((int) command[1]) + 5*((int) command[2]) + 7*((int) command[3]);
}