#include "CPC.h"

int write_channel_fd(int fd, message_t *data){
    struct msghdr msg_wr;
    struct iovec iov[1];
    struct cmsghdr *ptr = NULL;

    union{
        //使用这个结构体与真正要发的数据放在一起是为了使辅助数据的地址对齐  
        struct cmsghdr cm;
        char ctl[CMSG_SPACE(sizeof(int))];
    }ctl_un;

    switch(data->info.type) {
    case FD_TRANS:
        msg_wr.msg_control = ctl_un.ctl;
        msg_wr.msg_controllen = sizeof(ctl_un.ctl);

        ptr = CMSG_FIRSTHDR(&msg_wr);
        ptr->cmsg_len = CMSG_LEN(sizeof(int));
        ptr->cmsg_level = SOL_SOCKET;
        ptr->cmsg_type = SCM_RIGHTS;
        *((int *)CMSG_DATA(ptr)) = data->data.fd;

        iov[0].iov_base = (void *)(&(data->info));
        iov[0].iov_len = sizeof(info_t);
        break;
    case MSG_TRANS:
        msg_wr.msg_control = NULL;
        msg_wr.msg_controllen = 0;

        iov[0].iov_base = data;
        iov[0].iov_len = sizeof(message_t);
        break;
    }

    msg_wr.msg_name = NULL;
    msg_wr.msg_namelen = 0;

    msg_wr.msg_iov = iov;
    msg_wr.msg_iovlen = 1;

    return (sendmsg(fd, &msg_wr, 0));
}

int recv_channel(int fd, message_t *data){
    struct msghdr msg_rc;
    struct iovec iov[1];
    ssize_t n;

    union{  
        struct cmsghdr cm;  
        char ctl[CMSG_SPACE(sizeof(int))];  
    }ctl_un;  
  
    struct cmsghdr *ptr = NULL;  
    msg_rc.msg_control = ctl_un.ctl;  
    msg_rc.msg_controllen = sizeof(ctl_un.ctl);  
  
    msg_rc.msg_name = NULL;  
    msg_rc.msg_namelen = 0;  
  
    iov[0].iov_base = (void *)data;  
    iov[0].iov_len = sizeof(message_t);  
  
    msg_rc.msg_iov = iov;  
    msg_rc.msg_iovlen = 1;  
  
    if((n = recvmsg(fd, &msg_rc, 0)) < 0){      
        perror("recvmsg error");  
        return n;  
    }  
    else if(n == 0){  
    //如果子进程收到0字节数据, 表明另一端已经关闭了.   
    //这里的另一端指的是所有其他进程, 包括父进程  
    //如果某一个进程关闭了, 但还有其他进程持有该管道另一端, 则不会有0字节数据出现  
    //所以我们这里如果其它进程都没了, 此进程也结束.  
        fprintf(stderr, "peer close the socket\n");  
        exit(1);  
    }  
  
    if( ( ptr = CMSG_FIRSTHDR( &msg_rc ) ) != NULL     //!> now we need only one,  
      && ptr->cmsg_len == CMSG_LEN( sizeof( int ) )        //!> we should use 'for' when  
     )                                                                                //!> there are many fds  
     {  
         if( ptr->cmsg_level != SOL_SOCKET )  
         {  
             fprintf(stderr, "Ctl level should be SOL_SOCKET\n");  
             exit(EXIT_FAILURE);  
         }  
           
         if( ptr->cmsg_type != SCM_RIGHTS )  
         {  
             fprintf(stderr, "Ctl type should be SCM_RIGHTS\n");  
             exit(EXIT_FAILURE);  
         }  
           
         data->data.fd = *(int*)CMSG_DATA(ptr);    //!> get the data : the file des*  
     }  
     else  
     {  
         data->data.fd = -1;  
     }  
  
     return n;  
}  
  
void add_fd_to_epoll_in(int epollfd, int fd){  
    struct epoll_event event;  
    event.events = EPOLLIN;  
    event.data.fd = fd;  
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);  
}  
  
void child_run(int index, process_t *processes){  
    int n, k;  
    message_t out;  
    struct epoll_event events[MAX_EVENT];  
    int epollfd = epoll_create(10);  
  
    if(epollfd < 0){  
        perror("epoll_create error");  
        exit(-1);  
    }  
  
    out.info.type = MSG_TRANS;  
    out.info.sourceIndex = index;  
    out.data.fd = -1;  
    memset(out.data.str, '\0', 64);  
    snprintf(out.data.str, 63, "I'm %d, who was born later than you",index);  
  
    //记得关闭不用的端口  
    close(processes[index].channel[0]);  
    processes[index].channel[0] = -1;  
    for(k=0; k<index; k++){  
                //向之前就产生的子进程打招呼...  
        write_channel_fd(processes[k].channel[0], &out);  
                //在父进程中已经关闭了, 不用在关  
        //close(processes[k].channel[1]);  
        processes[k].channel[1] = -1;  
    }  
  
    add_fd_to_epoll_in(epollfd, processes[index].channel[1]);  
  
    for(;;){  
        n = epoll_wait(epollfd, events, MAX_EVENT, -1);  
        printf("epoll return :%d\n", n);  
        if(n < 0){  
            perror("epoll_wait error");  
            exit(-1);  
        }  
        else if(n == 0)  
            break;  
  
        if(events[0].events & EPOLLIN){  
            message_t msg;  
            int n;  
            n = recv_channel(events[0].data.fd, &msg);  
            if(n == 0){  
                close(processes[index].channel[1]);  
                continue;  
            }  
            int newfd, sourceIndex;  
            switch(msg.info.type){  
            case FD_TRANS:   
                //添加到processes里面去  
                newfd = msg.data.fd;  
                if(newfd < 0){  
                    fprintf(stderr, "child %d fail to receive fd\n", index);  
                    exit(-1);  
                }  
  
                sourceIndex = msg.info.sourceIndex;  
                processes[sourceIndex].channel[0] = newfd;  
                processes[sourceIndex].channel[1] = -1;  
            #ifdef DEBUG  
                printf("I am child %d, I have just received %d's fd, %d bytes\n", index, sourceIndex, n);  
            #endif  
                printf("I am child %d, I'm gonna say hello to child %d\n", index, sourceIndex);  
                out.info.type = MSG_TRANS;  
                out.info.sourceIndex = index;  
                out.data.fd = -1;  
                memset(out.data.str, '\0', 64);  
                snprintf(out.data.str, 63, "hello,I'm %d", index);  
                n = write_channel_fd(processes[sourceIndex].channel[0], &out);  
                printf("I am child %d, I send %d bytes to %d\n", index, n, sourceIndex);  
                break;  
            case MSG_TRANS:  
                sourceIndex = msg.info.sourceIndex;  
                printf("I'm child %d, I have just received a message %d bytes from %d: \n%s\n", index,n,sourceIndex, msg.data.str);  
                 break;  
            }  
        }  
    }  
}  
