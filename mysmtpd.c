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

static void handle_client(int fd);

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
    char command[5];
    command[4] = '\0';
    nb_read_line(nb, recvbuf);
    memcpy(command, recvbuf, 4);

    int command_hash = hash_command(command);
    if (command_hash == HELP || command_hash == EXPN) {
      send_formatted(fd, "502 Unsupported command: %s\n", command);
      continue;
    }

    switch (command_hash) {
      case HELO:
        // implement
        send_formatted(fd, "250 OK %s\n", my_uname.__domainname);
        break;
      case EHLO: 
        // implement
        printf("wow so exclusive\n");
        send_formatted(fd, "250 OK %s\n", my_uname.__domainname);
        break;
      case MAIL: 
        // implement
        printf("hmm... I know this....\n");
        break;
      case RCPT: 
        // implement
        printf("hmm... I know this....\n");
        break;
      case DATA: 
        // implement
        printf("hmm... I know this....\n");
        break;
      case RSET: 
        // implement
        printf("hmm... I know this....\n");
        send_formatted(fd, "250 OK\n");
        break;
      case VRFY: 
        // implement
        printf("hmm... I know this....\n");
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
  
  nb_destroy(nb);
}

int hash_command(char *command) {
  return ((int) command[0]) + 2*((int) command[1]) + 3*((int) command[2]) + 4*((int) command[3]);
}
