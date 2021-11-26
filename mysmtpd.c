#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

#define HELO 754
#define EHLO 757
#define MAIL 730
#define RCPT 792
#define DATA 710
#define RSET 791
#define VRFY 816
#define NOOP 793
#define QUIT 806

#define EXPN 797
#define HELP 758

typedef enum {
  GREET_NEXT,
  MAIL_NEXT,
  RCPT_NEXT,
  DATA_NEXT
} state_t;

state_t state;
user_list_t forward_paths;
int temp_file = -1;
char temp_file_name[6];

static void handle_client(int fd);

static void hello(int fd, char *domain);

static void mail(int fd);

static void recipient(int fd);

static int data(int fd, net_buffer_t nb);

static void verify(int fd);

static void initialize();

static void clear();

static int hash_command(char *command);

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

  struct utsname my_uname;
  uname(&my_uname);

  send_formatted(fd, "220 Connection Established\n");
  state = GREET_NEXT;

  while (1) {
    int len = nb_read_line(nb, recvbuf);

    // Check client input validity and length
    if (len == 0 || len == -1) break;
    char* line = strtok(recvbuf, "\r\n"); 
    if (strlen(line) == MAX_LINE_LENGTH) {
      send_formatted(fd, "500 Line is too long\n");
      continue;
    }

    char *command = strtok(line, " ");
    int hashed_command = hash_command(command);
    if  (hashed_command == HELP || hashed_command == EXPN) {
      send_formatted(fd, "502 Unsupported command: %s\n", command);
      continue;
    }

    switch (hashed_command) {
      case HELO:
        hello(fd, my_uname.__domainname);
        break;
      case EHLO:
        hello(fd, my_uname.__domainname);
        break;
      case MAIL: 
        mail(fd);
        break;
      case RCPT: 
        recipient(fd);
        break;
      case DATA: 
        if (data(fd, nb) == -1) goto quit;
        break;
      case RSET: 
        clear();
        state = MAIL_NEXT;
        send_formatted(fd, "250 OK\n");
        break;
      case VRFY: 
        verify(fd);
        break;
      case NOOP: 
        send_formatted(fd, "250 OK\n");
        break;
      case QUIT: 
        send_formatted(fd, "221 Closing transmission Channel\n");
        goto quit;
      default:    
        send_formatted(fd, "500 Invalid command: %s\n", command);
    }
  } 

  quit: ;
  clear();
  nb_destroy(nb);
}

void hello(int fd, char *domain) {
  if (state == GREET_NEXT) {
    if (strtok(NULL, " ")) {
    clear();
    state = MAIL_NEXT;
    send_formatted(fd, "250 %s\n", domain);
    return;
    } 
    send_formatted(fd, "550 No domain given\n");
  }
}

void mail(int fd) {
  // check that server was greeted and that no other mail transaction is in process
  if (state != MAIL_NEXT) {
    send_formatted(fd, "503 Bad sequence of commands\n");
    return;
  }

  char *param = strtok(NULL, " ");
  // Error, no parameter 
  if (!param) {
    send_formatted(fd, "550 No parameter found\n"); 
    return;
  }
  
  // Check that reverse-path is specified with correct prefix
  char prefix[6];
  prefix[5] = '\0';
  memcpy(prefix, param, 5);
  if (strcmp(prefix, "FROM:") != 0) {
    send_formatted(fd, "550 Unsupported parameter\n"); 
    return;
  }

  state = RCPT_NEXT;
  clear();
  initialize();
  send_formatted(fd, "250 OK\n");
}

void recipient(int fd) {
  if (state != RCPT_NEXT && state != DATA_NEXT) {
    send_formatted(fd, "503 Bad sequence of commands\n");
    return;
  }

  char *param = strtok(NULL, " ");
  // Error, no params found
  if (!param) {
    send_formatted(fd, "550 No parameters found\n");
    return;;
  }
  
  // Check that forward-path is specified with correct prefix and brackets
  char prefix[5];
  prefix[4] = '\0';
  memcpy(prefix, param, 4);
  if (strcmp(prefix, "TO:<") != 0 || param[strlen(param) - 1] !='>') {
    send_formatted(fd, "550 Unsupported parameter\n");
    return;
  }

  int user_length = strlen(param) - 5;
  char user[user_length + 1];
  user[user_length] = '\0';
  memcpy(user, param + 4, user_length);

  // Add user to forward path if valid
  if (is_valid_user(user, NULL)) {
    add_user_to_list(&forward_paths, user);
    state = DATA_NEXT;
    send_formatted(fd, "250 OK\n");
  } else {
    send_formatted(fd, "550 No such user here\n");
  } 
}

// returns 1 if process is successful, returns -1 if client closed connection
int data(int fd, net_buffer_t nb) {
  if (state != DATA_NEXT) {
    send_formatted(fd, "503 not greeted (Bad sequence of commands)\n");
    return 1;
  }

  send_formatted(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");

  char recvbuf[MAX_LINE_LENGTH + 1];
  char* line = "";
  int len;
  while (strcmp(line, ".") != 0) {
    len = nb_read_line(nb, recvbuf);
    // check line is valid and connection is intact
    if (len == 0 || len == -1) return -1;
    line = strtok(recvbuf, "\r\n");
    if (strlen(line) == MAX_LINE_LENGTH) {
      send_formatted(fd, "500 Line is too long\n");
      return 1;
    }

    write(temp_file, recvbuf, len);
    // write(temp_file, line, strlen(line));
    // write(temp_file, "\n", 1);
  }

  save_user_mail(temp_file_name, forward_paths);
  clear();
  state = MAIL_NEXT;
  send_formatted(fd, "250 OK\n");
  return 1;
}

void verify(int fd) {
  char *user = strtok(NULL, " ");
  if (is_valid_user(user, NULL)) {
    send_formatted(fd, "250 <%s>\n", user);
  } else {
    send_formatted(fd, "551 User not local\n");
  }
}

// Set up data structures for mail transaction
void initialize() {
  forward_paths = create_user_list();
  memcpy(temp_file_name, "XXXXXX", 6);;
  temp_file = mkstemp(temp_file_name);
}

// clears any mail transaction in progress
void clear() {
  destroy_user_list(forward_paths);
  if (temp_file != -1) {
    close(temp_file);  
    temp_file = -1;
  }
}

int hash_command(char *command) {
  if ( strlen(command) > 4 ) return -1;
  return ((int) command[0]) + 2*((int) command[1]) + 3*((int) command[2]) + 4*((int) command[3]);
}
