#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 1024

// AUTHORIZATION state commands
#define USER 1253
#define PASS 1271

// TRANSACTION state commands
#define STAT 1248
#define LIST 1298
#define RETR 1283
#define DELE 1138
#define RSET 1264

// Any state commands
#define NOOP 1270
#define QUIT 1289

// Unsupported commands
#define TOP 721
#define UIDL 1176
#define APOP 1260

// Enumeration for the state of the server
typedef enum {
    GREETING_STATE,
    AUTHORIZATION_STATE_USERNAME,
    AUTHORIZATION_STATE_PASSWORD,
    TRANSACTION_STATE,
    UPDATE_STATE
} state_t;

// Function declarations
static void handle_client(int fd);

bool command_user(int fd, char **user);

bool command_pass(int fd, char *user);

void command_stat(int fd, mail_list_t mail_list);

void command_list(int fd, mail_list_t mail_list, unsigned int original_mail_count);

void command_retr(int fd, mail_list_t mail_list, unsigned int original_mail_count);

void command_dele(int fd, mail_list_t mail_list);

void command_noop(int fd);

void command_rset(int fd, mail_list_t mail_list, unsigned int original_mail_count);

void command_quit(int fd, char *user, mail_list_t mail_list);

int hash_command(char *command);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\r\n", argv[0]);
        return 1;
    }

    run_server(argv[1], handle_client);

    return 0;
}

void handle_client(int fd) {
    // Initialize states
    char *user = malloc(MAX_LINE_LENGTH);
    mail_list_t user_mail_list;
    unsigned int original_mail_count;
    char recvbuf[MAX_LINE_LENGTH + 1];
    net_buffer_t nb = nb_create(fd, MAX_LINE_LENGTH);

    // Server greeting
    state_t state = GREETING_STATE;

    if (state == GREETING_STATE) {
        send_formatted(fd, "+OK POP3 server ready\r\n");
        state = AUTHORIZATION_STATE_USERNAME;
    }

    // While the client is connected
    while (1) {
        // Receive a line from the client
        int len = nb_read_line(nb, recvbuf);

        // Check client input validity and length
        if (len == 0 || len == -1) break;
        char *line = strtok(recvbuf, "\r\n");
        if (strlen(line) == MAX_LINE_LENGTH) {
            send_formatted(fd, "-ERR Line is too long\r\n");
            continue;
        }

        // Read the command
        char *command = strtok(line, " ");
        int hashed_command = hash_command(command);

        // Check if unsupported command
        if (hashed_command == TOP || hashed_command == UIDL || hashed_command == APOP) {
            send_formatted(fd, "-ERR Unsupported command: %s\r\n", command);
            continue;
        }

        // Handle the command
        switch (hashed_command) {
            case USER:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    memset(user, 0, MAX_LINE_LENGTH);
                    if (command_user(fd, &user)) {
                        // User is valid, go to password state
                        state = AUTHORIZATION_STATE_PASSWORD;
                    } else {
                        // User is invalid, go to username state
                        state = AUTHORIZATION_STATE_USERNAME;
                    }
                } else {
                    // User is already logged in, send an error
                    send_formatted(fd, "-ERR Already logged in!\r\n");
                }
                break;
            case PASS:
                if (state == AUTHORIZATION_STATE_USERNAME) {
                    // Valid user name not entered yet, send an error
                    send_formatted(fd, "-ERR Send USER command first with valid username\r\n");
                } else if (state == AUTHORIZATION_STATE_PASSWORD) {
                    if (command_pass(fd, user)) {
                        // Password is valid, grab the mail list and mail count
                        user_mail_list = load_user_mail(user);
                        original_mail_count = get_mail_count(user_mail_list);
                        // Go to transaction state
                        state = TRANSACTION_STATE;
                    } else {
                        // Password is invalid, go to username state
                        state = AUTHORIZATION_STATE_USERNAME;
                    }
                } else {
                    // User is already logged in, send an error
                    send_formatted(fd, "-ERR Already logged in!\r\n");
                }
                break;
            case STAT:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    // User is not logged in, send an error
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else {
                    // User is logged in, handle STAT command
                    command_stat(fd, user_mail_list);
                }
                break;
            case LIST:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    // User is not logged in, send an error
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else {
                    // User is logged in, handle the LIST command
                    command_list(fd, user_mail_list, original_mail_count);
                }
                break;
            case RETR:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    // User is not logged in, send an error
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else {
                    // User is logged in, handle the RETR command
                    command_retr(fd, user_mail_list, original_mail_count);
                }
                break;
            case DELE:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    // User is not logged in, send an error
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else {
                    // User is logged in, handle the DELE command
                    command_dele(fd, user_mail_list);
                }
                break;
            case RSET:
                if (state == AUTHORIZATION_STATE_USERNAME || state == AUTHORIZATION_STATE_PASSWORD) {
                    // User is not logged in, send an error
                    send_formatted(fd, "-ERR Login first using USER and PASS commands!\r\n");
                } else {
                    // User is logged in, handle the RSET command
                    command_rset(fd, user_mail_list, original_mail_count);
                }
                break;
            case NOOP:
                // NOOP command valid in any state, handle NOOP command
                command_noop(fd);
                break;
            case QUIT:
                if (state == TRANSACTION_STATE) {
                    // User is logged in, handle the QUIT command
                    state = UPDATE_STATE;
                    command_quit(fd, user, user_mail_list);
                } else {
                    // User is not logged in, just say bye
                    send_formatted(fd, "+OK POP3 Server signing off\r\n");
                }
                goto quit;
            default:
                // Unknown command, send an error
                send_formatted(fd, "-ERR Invalid command: %s\r\n", command);
        }

    }

    quit:;
    nb_destroy(nb);
}

// Process USER command: returns true if the username is valid, false otherwise
bool command_user(int fd, char **user) {
    // Read the username
    char *user_input = strtok(NULL, " ");

    // If username is missing, send an error
    if (user_input == NULL) {
        strncpy(*user, "", MAX_LINE_LENGTH);
        send_formatted(fd, "-ERR Mailbox name argument missing for USER command\r\n");
        return 0;
    }

    // Check if the username is valid
    if (is_valid_user(user_input, NULL)) {
        // Username is valid, store it and send OK message
        send_formatted(fd, "+OK %s is a valid mailbox\r\n", user_input);
        strncpy(*user, user_input, strlen(user_input));
        return 1;
    } else {
        // Username is not valid, clear user and send ERR message
        memset(*user, 0, MAX_LINE_LENGTH);
        send_formatted(fd, "-ERR No mailbox for %s here\r\n", user_input);
        return 0;
    }
}

// Process PASS command: returns true if the password is valid and updates user, false otherwise.
bool command_pass(int fd, char *user) {
    // Read the password
    char *pass_input = strtok(NULL, " ");

    // If password is missing, send an error
    if (pass_input == NULL) {
        send_formatted(fd, "-ERR No password provided, login again with USER command first\r\n");
        return 0;
    }

    // Check if the password is valid
    if (is_valid_user(user, pass_input)) {
        // Password is valid, send OK message
        send_formatted(fd, "+OK Logged in successfully, welcome %s! (%d new messages)\r\n", user,
                       get_mail_count(load_user_mail(user)));
        return 1;
    } else {
        // Password is not valid, send ERR message
        send_formatted(fd, "-ERR Invalid password, login again with USER command first\r\n");
        return 0;
    }
}

// Process STAT command
void command_stat(int fd, mail_list_t mail_list) {
    // Send the mail count and size (not including the deleted mails)
    send_formatted(fd, "+OK %d %zu\r\n", get_mail_count(mail_list),
                   get_mail_list_size(mail_list));
}

// Process LIST command
void command_list(int fd, mail_list_t mail_list, unsigned int original_mail_count) {
    // Get argument, if any
    char *msg_num_input = strtok(NULL, " ");

    // If no argument, list all messages
    if (msg_num_input == NULL) {
        send_formatted(fd, "+OK %d messages (%zu octets)\r\n",
                       get_mail_count(mail_list), get_mail_list_size(mail_list));
        for (int i = 0; i < original_mail_count; i++) {
            mail_item_t mail_item = get_mail_item(mail_list, i);
            if (mail_item != NULL) {
                send_formatted(fd, "%d %zu\r\n", i + 1, get_mail_item_size(mail_item));
            }
        }
        send_formatted(fd, ".\r\n");
        return;
    } else {
        // If argument, list only that message
        int msg_num = atoi(msg_num_input);
        mail_item_t mail_item = get_mail_item(mail_list, msg_num - 1);
        if (mail_item == NULL || msg_num < 1 || msg_num > original_mail_count) {
            // If message does not exist or is deleted, return error
            send_formatted(fd, "-ERR Message %d does not exist or deleted!\r\n", msg_num);
            return;
        }
        send_formatted(fd, "+OK %d %zu\r\n", msg_num, get_mail_item_size(mail_item));
    }
}

// Process RETR command
void command_retr(int fd, mail_list_t mail_list, unsigned int original_mail_count) {
    // Get the message number
    char *msg_num_input = strtok(NULL, " ");

    // If no message number was given, send an error
    if (msg_num_input == NULL) {
        send_formatted(fd, "-ERR No message number given!\r\n");
        return;
    } else {
        // Convert the message number to an integer and check if it is valid
        int msg_num = atoi(msg_num_input);
        mail_item_t mail_item = get_mail_item(mail_list, msg_num - 1);
        if (mail_item == NULL || msg_num < 1 || msg_num > original_mail_count) {
            send_formatted(fd, "-ERR Message %d does not exist or deleted!\r\n", msg_num);
            return;
        }

        // Send mail size and message
        send_formatted(fd, "+OK %zu octets\r\n", get_mail_item_size(mail_item));

        FILE *mail_item_data = get_mail_item_contents(mail_item);
        char buffer[MAX_LINE_LENGTH];
        while (fgets(buffer, MAX_LINE_LENGTH, mail_item_data) != NULL) {
            send_formatted(fd, "%s", buffer);
        }
        fclose(mail_item_data);

        // Send the end of message (.CRLF)
        send_formatted(fd, ".\r\n");
    }
}

// Process DELE command
void command_dele(int fd, mail_list_t mail_list) {
    // Get the message number
    char *msg_num_input = strtok(NULL, " ");

    // If no message number was given, send an error
    if (msg_num_input == NULL) {
        send_formatted(fd, "-ERR No message number given. Nothing deleted!\r\n");
        return;
    } else {
        // Convert the message number to an integer and check if it is valid
        int msg_num = atoi(msg_num_input);
        mail_item_t mail_item = get_mail_item(mail_list, msg_num - 1);
        if (mail_item == NULL || msg_num < 1) {
            send_formatted(fd, "-ERR Message %d already deleted or does not exist!\r\n", msg_num);
            return;
        }

        // Mark the mail as deleted
        mark_mail_item_deleted(mail_item);
        send_formatted(fd, "+OK Message %d deleted!\r\n", msg_num);
    }

}

// Process NOOP command
void command_noop(int fd) {
    // Send OK response
    send_formatted(fd, "+OK noop received!\r\n");
}

// Process RSET command
void command_rset(int fd, mail_list_t mail_list, unsigned int original_mail_count) {
    int deleted_count = original_mail_count - get_mail_count(mail_list);
    reset_mail_list_deleted_flag(mail_list);
    send_formatted(fd, "+OK %d message(s) restored!\r\n",
                   deleted_count);
}

// Process QUIT command
void command_quit(int fd, char *user, mail_list_t mail_list) {
    // Destroy all mail marked for deletion
    destroy_mail_list(mail_list);
    send_formatted(fd, "+OK POP3 Server signing off. Bye %s! (%d messages left)\r\n", user,
                   get_mail_count(mail_list));
}

// Helper to hash commands
int hash_command(char *command) {
    if (strlen(command) > 4) return -1;
    return ((int) command[0]) + 3 * ((int) command[1]) + 5 * ((int) command[2]) + 7 * ((int) command[3]);
}