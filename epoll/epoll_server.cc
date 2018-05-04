#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
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
#include <string.h>

//listen同时能处理的最大连接数
#define MAX_CONNECT_HANDLE 20

//epoll_wait获取的最大事件数
#define MAX_EVENTS 30

//端口号
#define PORT 55001

//缓冲区大小
#define BUFFSIZE 512

//-----------------------------------------------------------------------------
//定义连接句柄的数据结构体
typedef struct ST_Client
{
  int mi_fd;//连接句柄
  char mchsz_buf[BUFFSIZE + 1];
  int mi_size;//缓存区大小
  int mi_length;//已接受的字符串长度
  int mi_needLen;//需要接受的字符串长度
  bool bol_flag;//分段标志
  bool bol_first;//是否第一次接受数据

}ST_Client_Data;

//设置socket连接为非阻塞模式
void setnonblocking(int sockfd)
{
  int opts;
  opts = fcntl(sockfd,F_GETFL);
  if(opts < 0)
  {
    perror("fcntl(F_GETFL)\n");
    exit(1);
  }

  opts = (opts | O_NONBLOCK);

  if(fcntl(sockfd, F_SETFL, opts) < 0)
  {
    perror("fcntl(F_SETFL)\n");
    exit(1);
  }
}

int main()
{
  struct epoll_event st_listenev;
  struct epoll_event stsz_events[MAX_EVENTS];
  struct sockaddr_in st_serverlocal, st_clientSockIn;

  char chsz_putBuf[BUFFSIZE + 1] = {0};

  int i_fdListen, i_fdConnSock, i_epfd, i_nfds, i, i_fd;

  socklen_t socklen = sizeof(struct sockaddr_in);

  //创建listen socket
  if( (i_fdListen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("create socket...\n");
    exit(1);
  }
  //设置监听句柄为非阻塞 accept即为非阻塞
  setnonblocking(i_fdListen);

  //配置监听IP 端口
  bzero(&st_serverlocal, sizeof(st_serverlocal));
  st_serverlocal.sin_family = AF_INET;
  st_serverlocal.sin_addr.s_addr = htonl(INADDR_ANY);
  st_serverlocal.sin_port = htons(PORT);

  //将listen socket 与 监听IP 端口绑定
  if(bind(i_fdListen, (struct sockaddr*) &st_serverlocal, socklen) < 0)
  {
    perror("bind socket...\n");
    exit(1);
  }

  //开始监听，等待客户端连接
  if(listen(i_fdListen, MAX_CONNECT_HANDLE) == -1)
  {
    perror("listen socket...\n");
    exit(1);
  }

  //创建epoll句柄
  i_epfd = epoll_create(MAX_EVENTS);
  if(i_epfd == -1)
  {
    perror("epoll_create...\n");
    exit(EXIT_FAILURE);
  }


  //设置listen socket的监听事件，并加入epoll
  st_listenev.events = EPOLLIN;
  //st_listenev.data.fd = i_fdListen;
  ST_Client_Data* stptr_listenData = (ST_Client_Data*)calloc(1, sizeof(ST_Client_Data));
  if(stptr_listenData == NULL)
  {
    perror("calloc server data\n");
    exit(EXIT_FAILURE);
  }
  stptr_listenData->mi_fd = i_fdListen;
  stptr_listenData->mi_size = BUFFSIZE + 1;
  stptr_listenData->mi_length = 0;
  stptr_listenData->mi_needLen = 0;
  st_listenev.data.ptr = stptr_listenData;

  if(epoll_ctl(i_epfd, EPOLL_CTL_ADD, i_fdListen, &st_listenev) == -1)
  {
    perror("epoll_ctl:listen_sock");
    exit(EXIT_FAILURE);
  }


  //循环处理
  //Listen socket 为LT模式
  //Client socket 为ET模式
  for(;;)
  {
    //阻塞
    i_nfds = epoll_wait(i_epfd, stsz_events, MAX_EVENTS, -1);
    if(i_nfds == -1)
    {
      perror("epoll_pwait");
      exit(EXIT_FAILURE);
    }

    for(i = 0; i < i_nfds; ++i)
    {
      //i_fd = stsz_events[i].data.fd;
      i_fd = ((ST_Client_Data*)stsz_events[i].data.ptr)->mi_fd;

      //连接事件
      if(i_fd == i_fdListen)
      {
        printf("accept\n");

        //防止连接同一时间到达
        while( (i_fdConnSock = accept(i_fdListen, (struct sockaddr*) &st_clientSockIn, &socklen)) > 0 )
        {
          //设置连接句柄为非阻塞 recv send即为非阻塞
          setnonblocking(i_fdConnSock);

          //设置client socket 的监听事件并加入epoll中 ET模式
          struct epoll_event st_clientev;
          st_clientev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
          //st_clientev.data.fd = i_fdConnSock;
          ST_Client_Data* stptr_clientData = (ST_Client_Data*)calloc(1, sizeof(ST_Client_Data));
          if(stptr_clientData == NULL)
          {
            perror("calloc client data\n");
            exit(EXIT_FAILURE);
          }
          stptr_clientData->mi_fd = i_fdConnSock;
          stptr_clientData->mi_size = BUFFSIZE + 1;
          stptr_clientData->mi_length = 0;
          stptr_clientData->mi_needLen = 0;
          stptr_clientData->bol_flag = false;
          stptr_clientData->bol_first = false;
          st_clientev.data.ptr = stptr_clientData;

          if(epoll_ctl(i_epfd, EPOLL_CTL_ADD, i_fdConnSock, &st_clientev) == -1)
          {
            perror("epoll_ctl:add");
            exit(EXIT_FAILURE);
          }
        }
        //监听句柄设置为非阻塞模式，没有连接时accept会立即返回
        if(i_fdConnSock == -1)
        {
          if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
            perror("accept");
        }

        continue;

      }
      //客户端关闭close ctrl+c kill

      else if( (stsz_events[i].events & EPOLLIN) && (stsz_events[i].events & EPOLLRDHUP ))
      {
        printf("EPOLLIN | EPOLLRDHUP\n");
        printf("ev.events: %d\n", stsz_events[i].events);

        //释放空间
        free(stsz_events[i].data.ptr);
        close(i_fd);
      }
      //读事件
      else if(stsz_events[i].events & EPOLLIN)
      {
        printf("EPOLLIN\n");
        printf("ev.events: %d\n", stsz_events[i].events);

        ST_Client_Data* stptr_clientData = (ST_Client_Data*)stsz_events[i].data.ptr;
        if(stptr_clientData == NULL)
        {
          perror("指针异常...\n");
          close(i_fd);
          continue;
        }

        //当前获取到的字符串长度
        int n = stptr_clientData->mi_length;
        //需要接受的长度
        int len = stptr_clientData->mi_needLen;

        //判断是否分段，即是否第一次接受
        //非分段
        if(!stptr_clientData->bol_flag)
        {

          //第一次接受数据
          if(!stptr_clientData->bol_first)
          {
            int i_nread;

            //循环读取
            while((i_nread = recv(i_fd, stptr_clientData->mchsz_buf + n, BUFFSIZE, 0)) > 0)
            {
              n += i_nread;
            }

            if(i_nread == -1 && errno != EAGAIN)
            {
              perror("read error");
              free(stsz_events[i].data.ptr);
              close(i_fd);
              continue;
            }

            //获取发送的总长度
            char chsz_len[8 + 1] = {0};
            //报文类型
            char ch_type;
            //数据格式

            strncpy(chsz_len, stptr_clientData->mchsz_buf, 8);
            len = atoi(chsz_len);

            ch_type = stptr_clientData->mchsz_buf[8];

            //已接受的字符串长度
            stptr_clientData->mi_length = n;
            //需要接受的字符串长度
            stptr_clientData->mi_needLen = len;
            //报文类型

          }
          //不是第一次接受数据
          else
          {
            int i_nread;

            //循环读取
            while((i_nread = recv(i_fd, stptr_clientData->mchsz_buf + n, BUFFSIZE, 0)) > 0)
            {
              n += i_nread;
            }

            if(i_nread == -1 && errno != EAGAIN)
            {
              perror("read error");
              free(stsz_events[i].data.ptr);
              close(i_fd);
              continue;
            }

          }

          //已读取完
          if(n == len)
          {

            stptr_clientData->bol_first = false;
            stptr_clientData->bol_flag = false;
            stptr_clientData->mi_length = 0;
            stptr_clientData->mi_needLen = 0;

            printf("get: [%s]\n", stptr_clientData->mchsz_buf);
            //修改连接句柄的监听事件为 写事件
            stsz_events[i].events = EPOLLOUT | EPOLLRDHUP | EPOLLET;


            //struct epoll_event st_clientev;
            //st_clientev.data.fd = i_fd;
            //st_clientev.events = EPOLLOUT | EPOLLRDHUP | EPOLLET;
            if(epoll_ctl(i_epfd, EPOLL_CTL_MOD, i_fd, &stsz_events[i]) == -1)
            {
              perror("epoll_ctl:mod");
              exit(EXIT_FAILURE);
            }

          }
          //未读取完，下次继续读
          else
          {
            stptr_clientData->bol_first = true;

          }


        }
        //分段
        else
        {

        }


      }
      //写事件
      else if(stsz_events[i].events & EPOLLOUT)
      {
        printf("EPOLLOUT\n");
        printf("ev.events: %d\n", stsz_events[i].events);

        memset(chsz_putBuf, 0, sizeof(chsz_putBuf));
        sprintf(chsz_putBuf, "00000080%s", "{\"bodyzip\":\"0\",\"bodyencrypt\":\"1\",\"signature\":\"haole\",\"msgbody\":\"OKOKOK\"}");

        int i_dataSize = strlen(chsz_putBuf);
        int n = i_dataSize;
        int i_nwrite;

        //循环发送
        while(n > 0)
        {
          i_nwrite = send(i_fd, chsz_putBuf + i_dataSize - n, n, 0);

          if(i_nwrite < n)
          {
            //发送失败
            if(i_nwrite == -1 && errno != EAGAIN)
            {
              perror("write client socket...\n");
              close(i_fd);
            }

            //缓存区满下次继续写
            break;

          }

          printf("send: %s\n", chsz_putBuf);

          n -= i_nwrite;
        }


        //全部写完了，改为监听读事件
        if(n == 0)
        {
          printf("send OK!!\n");
          //st_clientev.data.fd = i_fd;
          //st_clientev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
          stsz_events[i].events = EPOLLIN | EPOLLRDHUP | EPOLLET;

          if(epoll_ctl(i_epfd, EPOLL_CTL_MOD, i_fd, &stsz_events[i]) == -1)
          {
            perror("epoll_ctl:mod");
            exit(EXIT_FAILURE);
          }

        }
        //下次继续写
        else
        {

        }
      }
      //服务器异常事件
      else
      {
        printf("ev.events: %d\n", stsz_events[i].events);
        perror("服务器异常\n");
        close(i_fd);
      }
    }

  }
  return 0;
}


