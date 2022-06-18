#include "http_conn.h"

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

void removefd(int epollfd, int clntfd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, clntfd, 0);
    close(clntfd);
}

void http_conn::init(int clntfd, const sockaddr_in&addr){
    m_clntfd = clntfd;
    m_clntaddr = addr;
    addfd(m_epollfd, clntfd);
    m_user_count++;
    cgi = 0;
    query_string = NULL;
}

int http_conn::get_line(){
    int i = 0 ;
    char c = '\0';
    int n;
    while((i<READ_BUFFER_SIZE-1)&&(c!='\n')){
        n = recv(m_clntfd, &c, 1, 0);
        if(n>0){
            if(c=='\r'){
                n = recv(m_clntfd, &c, 1, MSG_PEEK);
                if((n>0)&&(c=='\n'))recv(m_clntfd, &c, 1, 0);
                else c='\n';
            }
            m_read_buf[i] = c;
            i++;
        }
        else c='\n';
    }
    m_read_buf[i] = '\0';
    return (i);
}

void http_conn::process_read(){
    size_t i, j;
    i = 0;
    j = 0;
    int numchars = get_line();//返回request_line的长度
    while(!isspace(m_read_buf[i])&&(j<sizeof(m_method)-1)){
        m_method[j] = m_read_buf[i];
        i++;
        j++;
    }
    m_method[j] = '\0';

    //如果既不是POST请求也不是GET请求则返回501
    if(strcasecmp(m_method, "GET")&&strcasecmp(m_method, "POST")){//支持GET和POST请求
        not_implemented();
        return;
    }

    //POST请求
    if(strcasecmp(m_method, "POST")==0) cgi = 1;

    j = 0;
    //读完空格
    while(isspace(m_read_buf[i])&&(i<READ_BUFFER_SIZE))i++;

    //读url
    while(!isspace(m_read_buf[i])&&(j<sizeof(m_url)-1)&&(i<READ_BUFFER_SIZE)){
        m_url[j] = m_read_buf[i];
        i++;
        j++;
    }
    m_url[i] = '\0';

    if(strcasecmp(m_method, "GET")==0){
        query_string = m_url;
        while((*query_string!='?')&&(*query_string!='\0'))query_string++;

        //若果有？
        if(*query_string == '?'){
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(m_path, "httpdocs%s", m_url);

    if(m_path[strlen(m_path) - 1] == '/'){
        strcat(m_path, "test.html");
    }

    //这里开始是什么意思？
    if(stat(m_path, &st) == -1){
        while((numchars>0)&&strcmp("\n", m_read_buf))numchars = get_line();
        not_found();
    }
    else{
        if((st.st_mode & S_IFMT) == S_IFDIR){
            strcat(m_path, "/test.html");
        }

        if((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;
        
        if(!cgi)serve_file();
        else execute_cgi();
    }
}

void http_conn::not_implemented(){

}

void http_conn::not_found(){

}

void http_conn::bad_request(){

}

void http_conn::get_request(){

}

void http_conn::internal_error(){

}

void http_conn::serve_file(){

}

void http_conn::execute_cgi(){

}

void http_conn::process_write(){

}
