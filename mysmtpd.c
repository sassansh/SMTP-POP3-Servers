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

static void handle_client(int fd);

static void hello(int fd, char *domain);

static void mail(int fd);

static void recipient(int fd);

static void data(int fd, net_buffer_t nb);

static void verify(int fd);

static int hash_command(char *command);

int main(int argc, char *argv[]) {
  
  if (argc != 2) {
    fprintf(stderr, "Invalid arguments. Expected: %s <port>\r\n", argv[0]);
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

  send_formatted(fd, "220 Connection Established\r\n");
  state = GREET_NEXT;

  while (1) {
    int len = nb_read_line(nb, recvbuf);

    // Check client input validity and length
    if (len == 0 || len == -1) break;
    char* line = strtok(recvbuf, "\r\n"); 
    if (strlen(line) == MAX_LINE_LENGTH) {
      send_formatted(fd, "500 Line is too long\r\n");
      continue;
    }

    char *command = strtok(line, " ");
    int hashed_command = hash_command(command);
    if  (hashed_command == HELP || hashed_command == EXPN) {
      send_formatted(fd, "502 Unsupported command: %s\r\n", command);
      continue;
    }

    switch (hashed_command) {
      case HELO:
        hello(fd, my_uname.nodename);
        break;
      case EHLO:
        hello(fd, my_uname.nodename);
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
        state = MAIL_NEXT;
        send_formatted(fd, "250 OK\r\n");
        break;
      case VRFY: 
        verify(fd);
        break;
      case NOOP: 
        send_formatted(fd, "250 OK\r\n");
        break;
      case QUIT: 
        send_formatted(fd, "221 Closing transmission Channel\r\n");
        goto quit;
      default:    
        send_formatted(fd, "500 Invalid command: %s\r\n", command);
    }
  } 

  quit: ;
  nb_destroy(nb);
}

void hello(int fd, char *domain) {
  if (state == GREET_NEXT) {
    if (strtok(NULL, " ")) {
    state = MAIL_NEXT;
    send_formatted(fd, "250 %s\r\n", domain);
    return;
    } 
    send_formatted(fd, "550 No domain given\r\n");
  }
}

void mail(int fd) {
  // check that server was greeted and that no other mail transaction is in process
  if (state != MAIL_NEXT) {
    send_formatted(fd, "503 Bad sequence of commands\r\n");
    return;
  }

  char *param = strtok(NULL, " ");
  // Error, no parameter 
  if (!param) {
    send_formatted(fd, "501 No parameter found\r\n"); 
    return;
  }
  // Check that reverse-path is specified with correct prefix
  if (strncasecmp(param, "FROM:<", 4)  != 0 || param[strlen(param) - 1] !='>') {
    send_formatted(fd, "501 Unsupported parameter\r\n"); 
    return;
  }

  state = RCPT_NEXT;
  destroy_user_list(forward_paths);
  forward_paths = create_user_list();
  send_formatted(fd, "250 OK\r\n");
}

void recipient(int fd) {
  if (state != RCPT_NEXT && state != DATA_NEXT) {
    send_formatted(fd, "503 Bad sequence of commands\r\n");
    return;
  }

  char *param = strtok(NULL, " ");
  // Error, no params found
  if (!param) {
    send_formatted(fd, "501 No parameters found\r\n");
    return;;
  }
  // Check that forward-path is specified with correct prefix and brackets
  if (strncasecmp(param, "TO:<", 4) != 0 || param[strlen(param) - 1] !='>') {
    send_formatted(fd, "501 Unsupported parameter\r\n");
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
    send_formatted(fd, "250 OK\r\n");
  } else {
    send_formatted(fd, "550 No such user here\r\n");
  } 
}

// returns 1 if process is successful, returns -1 if client closed connection
void data(int fd, net_buffer_t nb) {
  if (state != DATA_NEXT) {
    send_formatted(fd, "503 not greeted (Bad sequence of commands)\r\n");
    return;
  }

  send_formatted(fd, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
  
  char temp_file_name[] = "Temp-XXXXXX";
  int temp_file = mkstemp(temp_file_name);

  char recvbuf[MAX_LINE_LENGTH + 1];
  int len = nb_read_line(nb, recvbuf);
  while (strcmp(recvbuf, ".\r\n") != 0 && len > 0) {
    write(temp_file, recvbuf, len);
    len = nb_read_line(nb, recvbuf);    
  }

  save_user_mail(temp_file_name, forward_paths);
  unlink(temp_file_name);
  close(temp_file);
  state = MAIL_NEXT;
  send_formatted(fd, "250 OK\r\n");
}

void verify(int fd) {
  char *user = strtok(NULL, " ");
  // Error, no parameter 
  if (!user) {
    send_formatted(fd, "501 No parameter found\r\n"); 
    return;
  }
  if (is_valid_user(user, NULL)) {
    send_formatted(fd, "250 <%s> is a valid user\r\n", user);
  } else {
    send_formatted(fd, "550 User <%s> not local\r\n", user);
  }
}

int hash_command(char *command) {
  if ( strlen(command) > 4 ) return -1;
  return ((int) command[0]) + 2*((int) command[1]) + 3*((int) command[2]) + 4*((int) command[3]);
}