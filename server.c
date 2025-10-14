#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG
#define MAXCLIENTS 20

struct clientInfo
{
  int sockfd;
  char nickname[32];
  int step;
  bool active;
};

bool serverSetup(int *sockfd, char *hoststring, char *portstring)
{
  //variable that will be filled with data
  struct addrinfo *res, *pInfo;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int addrinfo_status = getaddrinfo(hoststring, portstring, &hints, &res);
  if(addrinfo_status != 0)
  {
    printf("\nERROR: getaddrinfo Failed\n");
    printf("Returned: %d\n", addrinfo_status);
    return false;
  }

  #ifdef DEBUG
  printf("getaddrinfo Succeded!\n");
  #endif

  for(pInfo = res; pInfo != NULL; pInfo = pInfo->ai_next)
  {
    *sockfd = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
    if(*sockfd != -1)
    {
      break;
    }
  }

  if(*sockfd == -1)
  {
    printf("\nERROR: Socket creation Failed\n");
    printf("Returned: %d\n", *sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Socket creation Succeded!\n");
  #endif

  int opt = 1;
  setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int bind_status = bind(*sockfd, pInfo->ai_addr, pInfo->ai_addrlen);
  if(bind_status == -1)
  {
    printf("\nERROR: Bind Failed\n");
    printf("Returned: %d\n", bind_status);
    close(*sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Bind Succeded!\n");
  #endif

  int listen_status = listen(*sockfd, SOMAXCONN);
  if(listen_status == -1)
  {
    printf("\nERROR: Listen Failed\n");
    printf("Returned: %d\n", listen_status);
    close(*sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Listen Succeded!\n");
  #endif

  freeaddrinfo(res);
  return true;
}

bool nick_checks(char *nickname)
{
  if(strlen(nickname) > 12)
  {
    fprintf(stderr, "ERROR: NICK CANT BE LONGER THAN 12 CHARACTERS\n");
    return false;
  }

  for(int i = 0; nickname[i] == '\0'; i++)
  {
    char c = nickname[i];

    if(!(isalpha(c) || isdigit(c) || c == '_'))
    {
      fprintf(stderr, "ERROR: WRONG CHARACTERS\n");
      return false;
    }
  }

  return true;
}

int main(int argc, char *argv[]){
  
  /* Do more magic */
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  char *input = argv[1];
  char *sep = strchr(input, ':');
  
  if (!sep) {
    fprintf(stderr, "Error: input must be in host:port format\n");
    return 1;
  }
  
  // Allocate buffers big enough
  char hoststring[256];
  char portstring[64];
  
  // Copy host part
  size_t hostlen = sep - input;
  if (hostlen >= sizeof(hoststring)) {
    fprintf(stderr, "Error: hostname too long\n");
    return 1;
  }
  strncpy(hoststring, input, hostlen);
  hoststring[hostlen] = '\0';
  
  // Copy port part
  strncpy(portstring, sep + 1, sizeof(portstring) - 1);
  portstring[sizeof(portstring) - 1] = '\0';
  
  printf("TCP server on: %s:%s\n", hoststring,portstring);
  
  int sockfd;
  fd_set readfds;
  struct timeval tv;
  bool setup_status = serverSetup(&sockfd, hoststring, portstring);
  if(!setup_status)
  {
    return EXIT_FAILURE;
  }

  struct clientInfo client_table[MAXCLIENTS];
  memset(client_table, 0, sizeof(client_table));
  int maxfd = sockfd;

  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int select_status = select(maxfd + 1, &readfds, NULL, NULL, &tv);
    if(select_status == -1)
    {
      printf("ERROR: Select error\n");
      fflush(stdout);
      continue;
    }
    else if(select_status == 0)
    {
      //Nothing happend keep listening
      continue;
    }

    //New connection
    if(FD_ISSET(sockfd, &readfds))
    {
      int clientfd = accept(sockfd, NULL, NULL);
      if(clientfd < 0)
      {
        printf("ERROR: clientfd Failed\n");
        fflush(stdout);
        printf("Returned: %d\n", clientfd);
        fflush(stdout);
        continue;
      }

      //New client, put the client in table
      for(int i = 0; i < MAXCLIENTS; i++)
      {
        if(!client_table[i].active)
        {
          client_table[i].sockfd = clientfd;
          client_table[i].active = true;
          client_table[i].step = 0;
          FD_SET(clientfd, &readfds);
          if(clientfd > maxfd)
          {
            maxfd = clientfd;
          }
          break;
        }
      }
    }
  }

  close(sockfd);
  return 0;
}
