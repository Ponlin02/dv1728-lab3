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

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG



bool try_connect(int *sockfd, char *Desthost, char *Destport)
{
  //variable that will be filled with data
  struct addrinfo *res, *pInfo;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int addrinfo_status = getaddrinfo(Desthost, Destport, &hints, &res);
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
    #ifdef DEBUG
    printf("Socket retry");
    #endif
  }

  if(*sockfd == -1)
  {
    printf("\nERROR: Socket creation Failed\n");
    printf("Returned: %d\n", *sockfd);
    return false;
  }

  //Set options for socket
  struct timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  setsockopt(*sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  #ifdef DEBUG
  printf("Socket creation Succeded!\n");
  #endif

  int connect_status = connect(*sockfd, pInfo->ai_addr, pInfo->ai_addrlen);
  if(connect_status != 0)
  {
    printf("\nERROR: RESOLVE ISSUE\n");
    printf("Returned: %d\n", connect_status);
    return false;
  }

  #ifdef DEBUG
  printf("Connection Succeded!\n");
  #endif

  freeaddrinfo(res);
  return true;
}

ssize_t send_helper(int sockfd, const char* send_buffer)
{
  ssize_t bytes_sent = send(sockfd, send_buffer, strlen(send_buffer), 0);

  #ifdef DEBUG
  printf("\nBytes sent: %ld\n", bytes_sent);
  printf("CLIENT SENT:\n%s\n", send_buffer);
  #endif

  return bytes_sent;
}

ssize_t recv_helper(int sockfd, char* recv_buffer, size_t bufsize)
{
  ssize_t bytes_recieved = recv(sockfd, recv_buffer, bufsize - 1, 0);
  if(bytes_recieved > 0)
  {
    recv_buffer[bytes_recieved] = '\0';
  }

  #ifdef DEBUG
  printf("\nBytes recieved: %ld\n", bytes_recieved);
  printf("SERVER RESPONSE:\n%s", recv_buffer);
  #endif
  
  return bytes_recieved;
}

int main(int argc, char *argv[]){
  
  /* Do magic */
  if (argc < 3) {
    fprintf(stderr, "Usage: %s server:port NICK\n", argv[0]);
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

  char *nickname = argv[2];
  
  printf("TCP server on: %s:%s user: %s\n", hoststring,portstring, nickname);

  int sockfd;
  fd_set readfds;
  fd_set writefds;
  struct timeval tv;
  bool connection_status = try_connect(&sockfd, hoststring, portstring);
  if(!connection_status)
  {
    return EXIT_FAILURE;
  }

  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int select_status = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if(select_status == -1)
    {
      printf("ERROR: Select error\n");
      break;
    }
    else if(select_status == 0)
    {
      //No activity continue waiting
      continue;
    }

    if(FD_ISSET(STDIN_FILENO, &readfds))
    {
      char send_buffer[1024];
      fgets(send_buffer, sizeof(send_buffer), stdin);
      send_helper(sockfd, send_buffer);
    }

    if(FD_ISSET(sockfd, &readfds))
    {
      char recv_buffer[1024];
      ssize_t bytes_recieved = recv_helper(sockfd, recv_buffer, sizeof(recv_buffer));
      if(bytes_recieved == -1)
      {
        printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
        continue;
      }
      if(bytes_recieved == 0)
      {
        printf("Server closed the connection.\n");
        break;
      }
      printf("Server sent: %s", recv_buffer);
    }
  }
  
  close(sockfd);
  return 0;
}
