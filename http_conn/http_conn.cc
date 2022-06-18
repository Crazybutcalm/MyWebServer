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
        close_conn();
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
        close_conn();
        return ;
    }
    else{
        if((st.st_mode & S_IFMT) == S_IFDIR){//如果是目录,自动打开test.html
            strcat(m_path, "/test.html");
        }

        if((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            //S_IXUSR:文件所有者具可执行权限
		    //S_IXGRP:用户组具可执行权限
		    //S_IXOTH:其他用户具可读取权限 
            cgi = 1;
        
        if(!cgi)serve_file();//如果不是cgi就返回文件
        else execute_cgi();//如果是cgi那么就处理cgi
    }

    close_conn();

    return ;
}

void http_conn::not_implemented(){
    sprintf(m_write_buf, "HTTP/1.0 501 Method Not Implement\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/thml\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</TITLE></HEAD>\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<BODY><P>HTTP request method not supported.\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</BODY></HTML>\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::not_found(){
    sprintf(m_write_buf, "HTTP/1.0 404 Not Found\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/thml\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<HTML><TITLE>Not Found</TITLE>\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<BODY><P>The server could not fulfill\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "your request because the resource specified\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "is unavailable or nonexistent.\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</BODY></HTML>\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::bad_request(){
    sprintf(m_write_buf, "HTTP/1.0 400 Bad Request\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/thml\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<P>Your browser sent a bad request, ");
    sprintf(m_write_buf+strlen(m_write_buf), "such as a POST without a Content-Length.\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::get_request(){
    sprintf(m_write_buf, "HTTP/1.0 200 OK\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/thml\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::internal_error(){
    sprintf(m_write_buf, "HTTP/1.0 500 Internal Server Error\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/thml\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<P>Error prohibited CGI execution.\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::serve_file(){
    FILE* resource = NULL;

    resource = fopen(m_path, "r");
    if(resource==NULL)not_found();
    else{
        get_request();
        cat(resource);
    }
    fclose(resource);
}

void http_conn::cat(FILE* resource){
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)){
        send(m_clntfd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void http_conn::execute_cgi(){
    
}

void http_conn::close_conn(){
    if(m_clntfd!=-1){
        removefd(m_epollfd, m_clntfd);
        m_epollfd = -1;
        m_user_count--;
    }
}

void http_conn::process(){
    process_read();
}
