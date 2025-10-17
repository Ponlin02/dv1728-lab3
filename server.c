#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG
#define MAXCLIENTS 20

struct clientInfo
{
  int sockfd;
  char nickname[32];
  int step;
  bool active;
  time_t connect_time;
  char ip[INET6_ADDRSTRLEN];
  int port;
};

ssize_t send_helper(int sockfd, const char* send_buffer)
{
  ssize_t bytes_sent = send(sockfd, send_buffer, strlen(send_buffer), 0);

  #ifdef DEBUG
  printf("\nBytes sent: %ld\n", bytes_sent);
  printf("SERVER SENT:\n%s\n", send_buffer);
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
  printf("CLIENTS RESPONSE:\n%s", recv_buffer);
  #endif
  
  return bytes_recieved;
}

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

void msg_step_one(struct clientInfo *table, int i)
{
  char recv_buffer[1024];
  ssize_t bytes_recieved = recv_helper(table[i].sockfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved <= 0)
  {
    char error[] = "ERR client disconnected\n";
    send_helper(table[i].sockfd, error);
    printf("Disconnect\n");
    fflush(stdout);
    table[i].active = false;
    close(table[i].sockfd);
    return;
  }

  char nick_key[10];
  char nick[32];
  sscanf(recv_buffer, "%s %s", nick_key, nick);
  //printf("nick key: %s\n", nick_key);
  //printf("nickname: %s\n", nick);
  if(strstr(nick_key, "NICK") == NULL)
  {
    char error[] = "ERR Wrong NICK format\n";
    send_helper(table[i].sockfd, error);
    printf("ERROR: Wrong nick format\n");
    printf("nick_key was: %s\n", nick_key);
    fflush(stdout);
    table[i].active = false;
    close(table[i].sockfd);
    return;
  }
  if(!nick_checks(nick))
  {
    char error[] = "ERR nickname error\n";
    send_helper(table[i].sockfd, error);
    printf("ERROR: Wrong nickname\n");
    printf("nick was: %s\n", nick);
    fflush(stdout);
    table[i].active = false;
    close(table[i].sockfd);
    return;
  }

  strcpy(table[i].nickname, nick);
  char ok[] = "OK\n";
  send_helper(table[i].sockfd, ok);
  table[i].step = 2;
  return;
}

void msg_step_two(struct clientInfo *table, int senderIndex, char *hoststring, char *portstring, time_t server_start)
{
  char recv_buffer[256];
  ssize_t bytes_recieved = recv_helper(table[senderIndex].sockfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved <= 0)
  {
    char error[] = "ERROR client disconnected\n";
    send_helper(table[senderIndex].sockfd, error);
    printf("Disconnect\n");
    fflush(stdout);
    table[senderIndex].active = false;
    close(table[senderIndex].sockfd);
    return;
  }

  if(strncmp(recv_buffer, "MSG ", 4) == 0)
  {
    for(int si = 0; si < MAXCLIENTS; si++)
    {
      if(table[si].active && table[si].step == 2)
      {
        char send_buffer[1024];
        sprintf(send_buffer, "%s %s %s", "MSG", table[senderIndex].nickname, recv_buffer + 4);
        printf("%s", send_buffer + 4);
        send_helper(table[si].sockfd, send_buffer);
      }
    }
    return;
  }

  if(strncmp(recv_buffer, "Status", 6) == 0)
  {
    char send_buffer[1024];
    char index_list[128];
    index_list[0] = '\0';
    strcat(index_list, "[");

    int noActive = 0;
    bool first = true;

    for (int i = 0; i < MAXCLIENTS; i++)
    {
      if(table[i].active)
      {
        noActive++;
        char temp[8];
        if(!first)
        {
          strcat(index_list, ",");
        }
        snprintf(temp, sizeof(temp), "%d", i);
        strcat(index_list, temp);
        first = false;
      }
    }
    strcat(index_list, "]");

    time_t now = time(NULL);
    long uptime = (long)(now - server_start);
    
    sprintf(send_buffer, "CPSTATUS\nListenAddress:%s:%s\nClients <%d Clients, Integer %s\nUpTime  <%ld>\n\n", hoststring, portstring, noActive, index_list, uptime);
    printf("%s", send_buffer);
    send_helper(table[senderIndex].sockfd, send_buffer);
    return;
  }

  if(strncmp(recv_buffer, "Clients", 7) == 0)
  {
    char send_buffer[1024];
    sprintf(send_buffer, "CPCLIENTS\n");
    for(int i = 0; i < MAXCLIENTS; i++)
    {
      if(table[i].active)
      {
        char temp[128];
        time_t now = time(NULL);
        long seconds_connected = (long)(now - table[i].connect_time);
        sprintf(temp, "<%d> <%s> <%s:%d> <%ld>\n", i, table[i].nickname, table[i].ip, table[i].port, seconds_connected);
        strcat(send_buffer, temp);
      }
    }

    printf("%s", send_buffer);
    send_helper(table[senderIndex].sockfd, send_buffer);
    return;
  }

  if(strncmp(recv_buffer, "KICK", 4) == 0)
  {
    char send_buffer[1024];
    char kick_command[8];
    char nick_kick[64];
    char secret_str[64];
    sscanf(recv_buffer, "%s %s %s", kick_command, nick_kick, secret_str);
    if(strcmp(kick_command, "KICK") != 0)
    {
      return; 
    }
    if(strcmp(secret_str, "mfo:.ai?fqajdalf832!") != 0)
    {
      sprintf(send_buffer, "CPKICK: Wrong secret\n\n");
      printf("%s", send_buffer);
      send_helper(table[senderIndex].sockfd, send_buffer);
      return;
    }
    int kick_index = -1;
    for(int i = 0; i < MAXCLIENTS; i++)
    {
      if(strcmp(table[i].nickname, nick_kick) == 0)
      {
        kick_index = i;
        break;
      }
    }

    if(kick_index == -1)
    {
      sprintf(send_buffer, "CPKICK: %s not found\n\n", nick_kick);
      printf("%s", send_buffer);
      send_helper(table[senderIndex].sockfd, send_buffer);
      return;
    }

    //Kicking the NICK
    char kick_msg[256];
    sprintf(kick_msg, "KICKED by %s\n", table[senderIndex].nickname); //temp "\n"
    //printf("Client is sent: %s\n", kick_msg);
    send_helper(table[kick_index].sockfd, kick_msg);
    close(table[kick_index].sockfd);
    table[kick_index].active = false;

    //Inform sender that kicking was a success
    sprintf(send_buffer, "CPKICK: %s removed\n\n", table[kick_index].nickname);
    printf("%s", send_buffer);
    send_helper(table[senderIndex].sockfd, send_buffer);
    return;
  }
}

int main(int argc, char *argv[]){
  
  /* Do more magic */
  if (argc < 2) {
    fprintf(stderr, "Usage: %s server:port\n", argv[0]);
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
  time_t server_start = time(NULL);

  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    for (int i = 0; i < MAXCLIENTS; i++) 
    {
      if (client_table[i].active)
      {
        FD_SET(client_table[i].sockfd, &readfds);
      }
    }

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
      struct sockaddr_storage client_addr;
      socklen_t addr_size = sizeof(client_addr);

      int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
      if(clientfd < 0)
      {
        printf("ERROR: clientfd Failed\n");
        fflush(stdout);
        printf("Returned: %d\n", clientfd);
        fflush(stdout);
        continue;
      }

      char ip_str[INET6_ADDRSTRLEN];
      int port;
      time_t time_of_connection = time(NULL);

      if(client_addr.ss_family == AF_INET)
      {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
      }
      else
      {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ip_str, sizeof(ip_str));
      }

      //New client, put the client in table
      for(int i = 0; i < MAXCLIENTS; i++)
      {
        if(!client_table[i].active)
        {
          client_table[i].sockfd = clientfd;
          client_table[i].active = true;
          client_table[i].step = 1;
          strcpy(client_table[i].ip, ip_str);
          client_table[i].port = port;
          client_table[i].connect_time = time_of_connection;
          FD_SET(clientfd, &readfds);
          if(clientfd > maxfd)
          {
            maxfd = clientfd;
          }
          break;
        }
      }

      //Contact the client and begin chat protocol
      char *hello = "HELLO 1\n";
      send_helper(clientfd, hello);
    }

    //Existing connection
    for(int i = 0; i < MAXCLIENTS; i++)
    {
      if(client_table[i].active && FD_ISSET(client_table[i].sockfd, &readfds))
      {
        if(client_table[i].step == 2)
        {
          msg_step_two(client_table, i, hoststring, portstring, server_start);
        }
        if(client_table[i].step == 1)
        {
          msg_step_one(client_table, i);
        }
      }
    }
  }

  close(sockfd);
  return 0;
}
