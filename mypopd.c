#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

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

#define TOP 721
#define UIDL 1176
#define APOP 1260

typedef enum {
    GREETING_STATE,
    AUTHORIZATION_STATE_USERNAME,
    AUTHORIZATION_STATE_PASSWORD,
    TRANSACTION_STATE,
    UPDATE_STATE
} state_t;

static void handle_client(int fd);

int hash_command(char *command);

void clear();

bool command_user(int fd, char **user);

bool command_pass(int fd, char *user);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\r\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {

    char *user = malloc(MAX_LINE_LENGTH);
    mail_list_t user_mail_list;

    state_t state = GREETING_STATE;

    char recvbuf[MAX_LINE_LENGTH + 1];
    net_buffer_t nb = nb_create(fd, MAX_LINE_LENGTH);

    if (state == GREETING_STATE) {
        send_formatted(fd, "+OK POP3 server ready\r\n");
        state = AUTHORIZATION_STATE_USERNAME;
    }

    while (1) {
        int len = nb_read_line(nb, recvbuf);

        // Check client input validity and length
        if (len == 0 || len == -1) break;
        char *line = strtok(recvbuf, "\r\n");
        if (strlen(line) == MAX_LINE_LENGTH) {
            send_formatted(fd, "-ERR Line is too long\r\n");
            continue;
        }

        char *command = strtok(line, " ");
        int hashed_command = hash_command(command);
        if (hashed_command == TOP || hashed_command == UIDL || hashed_command == APOP) {
            send_formatted(fd, "-ERR Unsupported command: %s\r\n", command);
            continue;
        }

        switch (hashed_command) {
            case USER:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    memset(user, 0, MAX_LINE_LENGTH);
                    if (command_user(fd, &user)) {
                        state = AUTHORIZATION_STATE_PASSWORD;
                    }
                } else {
                    send_formatted(fd, "-ERR Already logged in!\r\n");
                }
                break;
            case PASS:
                if (state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Send USER command first with valid username\r\n");
                } else if (state == AUTHORIZATION_STATE_PASSWORD) {
                    if (command_pass(fd, user)) {
                        user_mail_list = load_user_mail(user);
                        state = TRANSACTION_STATE;
                    } else {
                        memset(user, 0, MAX_LINE_LENGTH);
                        state = AUTHORIZATION_STATE_USERNAME;
                    }
                } else {
                    send_formatted(fd, "-ERR Already logged in!\r\n");
                }
                break;
            case STAT:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else if (state == TRANSACTION_STATE) {
                    send_formatted(fd, "+OK STAT received!\r\n");
                }
                break;
            case LIST:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else if (state == TRANSACTION_STATE) {
                    send_formatted(fd, "+OK LIST received!\r\n");
                }
                break;
            case RETR:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else if (state == TRANSACTION_STATE) {
                    send_formatted(fd, "+OK RETR received!\r\n");
                }
                break;
            case DELE:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else if (state == TRANSACTION_STATE) {
                    send_formatted(fd, "+OK DELE received!\r\n");
                }
                break;
            case RSET:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_USERNAME) {
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else if (state == TRANSACTION_STATE) {
                    clear();
                    send_formatted(fd, "+OK RSET received!\r\n");
                }
                break;
            case NOOP:
                send_formatted(fd, "+OK noop received!\r\n");
                break;
            case QUIT:
                if (state == TRANSACTION_STATE) {
                    send_formatted(fd, "+OK POP3 Server signing off. Bye %s!\r\n", user);
                    state = UPDATE_STATE;
                } else {
                    send_formatted(fd, "+OK POP3 Server signing off\r\n");
                }

                goto quit;
            default:
                send_formatted(fd, "-ERR Invalid command: %s\r\n", command);
        }

    }

    quit:;
    clear();
    nb_destroy(nb);
}

// Process the PASS command
bool command_pass(int fd, char *user) {
    char *pass_input = strtok(NULL, " ");

    if (pass_input == NULL) {
        send_formatted(fd, "-ERR No password provided, login again with USER command first\r\n");
        return 0;
    }

    if (is_valid_user(user, pass_input)) {
        send_formatted(fd, "+OK Logged in successfully, welcome %s! (%d new messages)\r\n", user,
                       get_mail_count(load_user_mail(user)));
        return 1;
    } else {
        send_formatted(fd, "-ERR Invalid password, login again with USER command first\r\n");
        return 0;
    }
}

// Process the USER command
bool command_user(int fd, char **user) {
    char *user_input = strtok(NULL, " ");

    if (user_input == NULL) {
        send_formatted(fd, "-ERR Mailbox name argument missing for USER command\r\n");
        return 0;
    }

    if (is_valid_user(user_input, NULL)) {
        send_formatted(fd, "+OK %s is a valid mailbox\r\n", user_input);
        strncpy(*user, user_input, strlen(user_input));
        return 1;
    } else {
        send_formatted(fd, "-ERR No mailbox for %s here\r\n", user_input);
        return 0;
    }
}

// undelete messages
void clear() {

}

int hash_command(char *command) {
    if (strlen(command) > 4) return -1;
    return ((int) command[0]) + 3 * ((int) command[1]) + 5 * ((int) command[2]) + 7 * ((int) command[3]);
}