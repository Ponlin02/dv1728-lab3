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
#include <termios.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG



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

void handleMessage(char *recv_buffer)
{
  char *msg = recv_buffer;

  while((msg = strstr(msg, "MSG")) != NULL)
  {
    char *end = strchr(msg, '\n');
    if(end == NULL)
    {
      break; //Incomplete message..
    }
    *end = '\0';

    char *space = strchr(msg + 4, ' ');
    if(space)
    {
      memmove(msg, space + 1, strlen(space));
    }
    
    printf("%s\n", msg);
    fflush(stdout);
  }
}

bool chat_protocol(int sockfd, char *nickname)
{
  char recv_buffer[1024];
  ssize_t bytes_recieved = recv_helper(sockfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved == -1)
  {
    fprintf(stderr, "ERROR: MESSAGE LOST (TIMEOUT)\n");
    return false;
  }

  if(strstr(recv_buffer, "HELLO 1\n") == NULL)
  {
    fprintf(stderr, "ERROR: MISSMATCH PROTOCOL\n");
    return false;
  }

  char send_buffer[1024];
  sprintf(send_buffer, "%s %s\n", "NICK", nickname);
  ssize_t bytes_sent = send_helper(sockfd, send_buffer);
  if(bytes_sent == -1)
  {
    fprintf(stderr, "ERROR: COULDNT SEND MESSAGE\n");
    return false;
  }

  ssize_t bytes_recieved2 = recv_helper(sockfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved2 == -1)
  {
    fprintf(stderr, "ERROR: MESSAGE LOST (TIMEOUT)\n");
    return false;
  }

  if(strstr(recv_buffer, "OK\n") == NULL)
  {
    fprintf(stderr, "ERROR: INVALID NICK\n");
    return false;
  }

  printf("Name accepted!\n");
  fflush(stdout);
  handleMessage(recv_buffer + 3);
  fflush(stdout);
  return true;
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
  fflush(stdout);

  int sockfd;
  fd_set readfds;
  struct timeval tv;
  bool connection_status = try_connect(&sockfd, hoststring, portstring);
  if(!connection_status)
  {
    return EXIT_FAILURE;
  }

  bool nick_status = nick_checks(nickname);
  if(!nick_status)
  {
    close(sockfd);
    return EXIT_FAILURE;
  }

  bool chat_status = chat_protocol(sockfd, nickname);
  if(!chat_status)
  {
    close(sockfd);
    return EXIT_FAILURE;
  }

  /*struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);*/

  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int select_status = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if(select_status == -1)
    {
      printf("ERROR: Select error\n");
      //tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      close(sockfd);
      return EXIT_FAILURE;
    }
    else if(select_status == 0)
    {
      //No activity continue waiting
      continue;
    }

    if(FD_ISSET(STDIN_FILENO, &readfds))
    {
      char send_buffer[1024];
      char input[256];
      fgets(input, sizeof(input), stdin);
      sprintf(send_buffer, "%s %s %s", "MSG", nickname, input);
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
        //tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        close(sockfd);
        return EXIT_FAILURE;
      }
      handleMessage(recv_buffer);
    }
  }

  //tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  close(sockfd);
  return 0;
}
