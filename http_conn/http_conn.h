#ifndef HTTP_CONN_H
#define HTTP_CONN_H

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
// #include<sys/wait.h>
// #include<stdlib.h>
class http_conn{
public:
    http_conn();
    ~http_conn();

private:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

public:
    static int m_epollfd;//同一个EPOLL内核
    static int m_user_count;//连接数量

public:
    //初始化新的连接
    void init(int clntfd, const sockaddr_in&addr);
    //外部调用
    void process();

private:
    void process_read();
    // void process_write();
    int get_line();
    void not_implemented();
    void not_found();
    void bad_request();
    void get_request();
    void internal_error();
    void serve_file();
    void execute_cgi();
    void close_conn();
    void cat(FILE*);
private:
    int m_clntfd;
    sockaddr_in m_clntaddr;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    char m_method[255];
    char m_url[255];
    char m_path[512];
    char *query_string;
    int cgi;
    struct stat st;
};

#endif