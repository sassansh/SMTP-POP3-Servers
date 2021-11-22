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

state_t state = GREET_NEXT;
user_list_t forward_paths;
int temp_file = -1;
char temp_file_name[6];

static void handle_client(int fd);

static void hello(int fd, char* body, char *domain);

static void mail(int fd, char* body);

static void recipient(int fd, char* body);

static void data(int fd, char* body, net_buffer_t nb);

static void verify(int fd, char* body);

static char *get_param(char* buf);

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

  /* TO BE COMPLETED BY THE STUDENT */
  send_formatted(fd, "220 Connection Established\n");

  while (1) {
    nb_read_line(nb, recvbuf);
    char* command = get_param(recvbuf);

    int command_hash = hash_command(command);
    if (command_hash == HELP || command_hash == EXPN) {
      send_formatted(fd, "502 Unsupported command: %s\n", command);
      continue;
    }

    switch (command_hash) {
      case HELO:
        hello(fd, recvbuf + 4, my_uname.__domainname);
        break;
      case EHLO:
        hello(fd, recvbuf + 4, my_uname.__domainname);
        break;
      case MAIL: 
        mail(fd, recvbuf + 4);
        break;
      case RCPT: 
        recipient(fd, recvbuf + 4);
        break;
      case DATA: 
        data(fd, recvbuf + 4, nb);
        break;
      case RSET: 
        if (get_param(NULL) != NULL) {
          send_formatted(fd, "501 Syntax error in parameter or arg\n");
        } else {
          clear();
          send_formatted(fd, "250 OK\n");
        }
        break;
      case VRFY: 
        verify(fd, recvbuf + 4);
        break;
      case NOOP: 
        send_formatted(fd, "250 OK\n");
        break;
      case QUIT: 
        if (get_param(NULL) != NULL) send_formatted(fd, "501 Syntax error in parameter or arg\n");
        else {
          clear();
          send_formatted(fd, "221 Closing transmission Channel\n");
          goto quit;
        }
        break;
      default:    
        send_formatted(fd, "500 Invalid command: %s\n", command);
    }
  } 
  quit: ;
  
  nb_destroy(nb);
}

void hello(int fd, char* body, char *domain) {
  if (get_param(NULL)) {
    if (!get_param(NULL)) {
      clear();
      state = MAIL_NEXT;
      send_formatted(fd, "250 %s\n", domain);
      return;
    }
  } 
  send_formatted(fd, "550 no domain given\n");
}

void mail(int fd, char* body) {
  // E: 552, 451, 452, 550, 553, 503, 455, 555

  // check that server was greeted and that no other transaction is in process
  if (state != MAIL_NEXT || temp_file != -1) {
    send_formatted(fd, "503 not greeted (Bad sequence of commands)\n");
    return;
  }

  char *param = get_param(NULL);
  // Error, no params found
  if (!param) send_formatted(fd, "501 no mail\n");
  // Error if more than one param found
  if (get_param(NULL)) send_formatted(fd, "550 more than one\n");
  
  // Check that reverse-path is specified with correct prefix and brackets
  char prefix[7];
  prefix[6] = '\0';
  memcpy(prefix, param, 6);
  if (strcmp(prefix, "FROM:<") != 0 || param[strlen(param) - 1]!='>') {
    send_formatted(fd, "550 no domain given\n");   // might not need to enforce brackets
    return;
  }
    

  clear();
  state = RCPT_NEXT;
  initialize();
  send_formatted(fd, "250 OK\n");
}

void recipient(int fd, char* body) {
  if (state != RCPT_NEXT && state != DATA_NEXT) {
    send_formatted(fd, "503 not greeted (Bad sequence of commands)\n");
    return;
  }

  char *param = get_param(NULL);
  // Error, no params found
  if (!param) send_formatted(fd, "501 no mail\n");
  // Error if more than one param found
  if (get_param(NULL)) send_formatted(fd, "550 no domain given\n");
  
  // Check that reverse-path is specified with correct prefix and brackets
  char prefix[5];
  prefix[4] = '\0';
  memcpy(prefix, param, 4);
  if (strcmp(prefix, "TO:<") != 0 || param[strlen(param) - 1] !='>') {
    send_formatted(fd, "550 no domain given\n");
    return;
  }
    

  int user_length = strlen(param) - 5;
  char user[user_length + 1];
  user[user_length] = '\0';
  memcpy(user, param + 4, user_length);

  printf("user:%s\n", user);
  // only handle those in users.txt
  if (is_valid_user(user, NULL)) {
    add_user_to_list(&forward_paths, user);
    state = DATA_NEXT;
    send_formatted(fd, "250 OK\n");
  } else {
    send_formatted(fd, "550 No such user here\n");
  } 
}

void data(int fd, char* body, net_buffer_t nb) {
  if (state != DATA_NEXT) {
    send_formatted(fd, "503 not greeted (Bad sequence of commands)\n");
    return;
  }

  if (get_param(NULL) != NULL) {
    send_formatted(fd, "501 Syntax error in parameter or arg");
    return;
  }

  send_formatted(fd, "354 Start mail input; end with <CRLF>.<CRLF>\n");

  char recvbuf[MAX_LINE_LENGTH + 1];
  nb_read_line(nb, recvbuf);  // runtime check this runs properly
  char* line = strtok(recvbuf, "\r\n");
  while (strcmp(line, ".") != 0) {
    write(temp_file, line, strlen(line));
    write(temp_file, "\n", 1);
    nb_read_line(nb, recvbuf);
    line = strtok(recvbuf, "\r\n");
  }

  save_user_mail(temp_file_name, forward_paths);
  clear();
  state = MAIL_NEXT;
  send_formatted(fd, "250 OK\n");
}

void verify(int fd, char* body) {
  if (is_valid_user(get_param(NULL), NULL)) {
    // S: 250, 251, 252
    send_formatted(fd, "250 <%s>\n", body);
  } else {
    // E: 550, 551, 553, 502, 504
    send_formatted(fd, "551 User not local\n");
  }
}

/** 
 * if buf is not null, returns the first argument and saves the rest internally
 * subsequent calls with NULL as a parameter returns the next argument parsed in buf
 * Returns: parameters seperated by SP, 
 */
char *get_param(char* buf) {
  if (buf) {
    char* str = strtok(buf, "\r\n"); 
    return strtok(str, " ");
  } 
  return strtok(NULL, " ");
}

void initialize() {
  forward_paths = create_user_list();
  memcpy(temp_file_name, "XXXXXX", 6);;
  temp_file = mkstemp(temp_file_name);

}

void clear() {
  state = GREET_NEXT;
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
