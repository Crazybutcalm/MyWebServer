#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>
#include<ctype.h>
#include<string.h>
#include<stdio.h>
#include<sys/stat.h>
#include<assert.h>
#include "lock/lock.h"
#include "threadpool/threadpool.h"
#include "http_conn/http_conn.h"
#include<errno.h>

# define MAX_EVENT_NUMBER  10000
# define MAX_FD 65536

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int clntfd){
    epoll_event event;
    event.data.fd = clntfd;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clntfd, &event);
    setnonblocking(clntfd);
}


int main(void){
    int serv_fd;
    unsigned short port = 9010;
    sockaddr_in serv_addr;
    serv_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(serv_fd>=0);

    int option;
    socklen_t optlen;
    optlen = sizeof(option);
    option = 1;
    setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    int ret = 0;
    ret = bind(serv_fd, (sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret>=0);

    ret = listen(serv_fd, 5);
    assert(ret>=0);
    
    //创建线程池
    threadpool<http_conn>* pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }
    catch(...){
        return 1;
    }

    //预先为每一个客户端分配一个http_conn对象
    http_conn* users = new http_conn [MAX_FD];


    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd!=1);
    addfd(epollfd, serv_fd);
    http_conn::m_epollfd = epollfd;

    while(1){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number<0)&&(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++){
            if(serv_fd == events[i].data.fd){
                sockaddr_in clnt_addr;
                socklen_t clnt_len = sizeof(clnt_addr);
                int clnt_fd = accept(serv_fd, (sockaddr*)&clnt_addr, &clnt_len);
                if(clnt_fd<0){
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD){
                    const char* info = "Internal Server Busy";
                    send(clnt_fd, info, strlen(info), 0);
                    close(clnt_fd);
                    continue;
                }
                users[clnt_fd].init(clnt_fd, clnt_addr);
            }
            else{
                pool->append(users + events[i].data.fd);
            }
        }
    }
    close(epollfd);
    close(serv_fd);
    delete [] users;
    delete pool;
    users = NULL;
    return 0;
}