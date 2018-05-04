#include <stdio.h>  
#include <string.h>  
#include <stdlib.h>  
#include <sys/epoll.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/socket.h>  

#ifndef CPC_H  
#define CPC_H  

#define DEBUG  
#define MAX_EVENT 64  
//消息分两种, 一种是传递描述符; 一种在验证的时候我们会发送字符串验证  
enum message_type{FD_TRANS = 0, MSG_TRANS};  

//做个规定, socketpair的数组, 0给父进程用. 1给子进程用  
//所需的关于每个进程的结构体  
typedef struct {  
  pid_t pid;  
  int index;  
  int channel[2];  
}process_t;  

typedef struct {  
  enum message_type type;  
  //消息是来自哪个进程的  
  int sourceIndex;  
}info_t;  

typedef struct{  
  //传递描述符用这个  
  int fd;  
  //传递字符串用这个  
  char str[64];  
}content_t;  

typedef struct {  
  info_t info;  
  content_t data;  
}message_t;  

//每个子进程主函数体  
void child_run(int index, process_t *processes);  
//消息读写函数  
int write_channel_fd(int fd, message_t *data);  
int recv_channel(int fd, message_t *data);  
//添加事件到epoll中  
void add_fd_to_epoll_in(int epollfd, int fd);  

#endif  
