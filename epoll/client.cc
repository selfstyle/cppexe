#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 55001
#define BUFFSIZE 512

int main()  {
  int sockfd;
  int rc;
  struct sockaddr_in servaddr;

  char buf[BUFFSIZE + 1] = {0};
  char getbuf[BUFFSIZE + 1] = {0};

  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_aton("127.0.0.1",&servaddr.sin_addr);
  //servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(PORT);

  sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd == -1)
  {
    perror("create socket failed");
    exit(0);
  }
  rc = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  if(rc == -1)
  {
    perror("connect error");
    exit(0);
  }

  //sleep(3);

  sprintf(buf, "0000002000");

  rc = send(sockfd, buf, strlen(buf), 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }

  sleep(3);
  sprintf(buf, "AABBCCDDEE");
  rc = send(sockfd, buf, strlen(buf), 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }

  rc = recv(sockfd, getbuf, BUFFSIZE, 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }

  printf("get : [%s]\n", getbuf);

  sprintf(buf, "0000002000");

  rc = send(sockfd, buf, strlen(buf), 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }

  sleep(3);
  sprintf(buf, "AABBCCDDEE");
  rc = send(sockfd, buf, strlen(buf), 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }


  rc = recv(sockfd, getbuf, BUFFSIZE, 0);
  if(rc == -1)
  {
    perror("send error");
    exit(0);
  }

  printf("get : [%s]\n", getbuf);


  sleep(3);
  close(sockfd);
  return 0;
}
