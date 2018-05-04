//目的是在对父进程发送一个信号后, 每个子进程会向其他子同级别的子进程发送一句问候.  
//传入进程个数. 不传的话默认是2个  
#include "CPC.h"  
  
int main(int ac, char *av[])  
{  
    int numOfProcesses, i, j;  
    process_t *processes;  
    size_t n;  
    pid_t pid;  
  
    if(ac == 2)  
        numOfProcesses = atoi(av[1]);  
    else if(ac == 1)  
        numOfProcesses = 2;  
    else{  
        fprintf(stderr, "Usage : %s num\n", av[0]);  
        exit(-1);  
    }  
      
    processes = (process_t *)malloc(sizeof(process_t) * numOfProcesses);  
    for(i=0; i<numOfProcesses; i++)  
        processes[i].index = -1;  
  
    for(i=0; i<numOfProcesses; i++){  
        message_t msg;  
        if(socketpair(AF_UNIX, SOCK_STREAM, 0, processes[i].channel) < 0){      
            perror("socketpair error");  
            exit(-1);  
        }  
  
        pid = fork();  
        switch(pid){  
            case -1:  
                perror("fork error");  
                exit(-1);  
            case 0:  
                //记得在子进程里关掉其他子进程的管道某端  
                child_run(i, processes);  
                exit(1);  
            default:  
                break;  
        }  
  
        processes[i].pid = pid;  
        processes[i].index = i;  
  
        msg.info.type = FD_TRANS;  
        msg.info.sourceIndex = i;  
        msg.data.fd = processes[i].channel[0];        //父进程通过它和某子进程的管道 把 父进程和某子进程的通信口 传过去  
        //给之前所有子进程发送这个口  
        for(j=0; j<i; j++){  
        #ifdef DEBUG  
            printf("As father, I just want to send child %d's fd to child %d\n",i, j);  
        #endif  
            //传递每个描述符  
            //父子进程会不会同时向同一个子进程传递数据?  
            //因为父进程用0向子进程传数据, 所起其他子进程也用这个0来传  
            n = write_channel_fd(processes[j].channel[0], &msg);  
            close(processes[j].channel[1]);  
        #ifdef DEBUG  
            printf("As father, I have just sent child %d's fd to child %d , %d bytes\n",i, j, (int)n);  
        #endif  
        }  
    }  
  
    //以上, 就已经将numOfProcesses个子进程连接起来了. 下面, 再实现一个功能.  
    //父进程收到SIGINT信号后, 让子进程退出  
 
    return 0;
}
