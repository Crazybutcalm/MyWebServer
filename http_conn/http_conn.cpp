#include "http_conn.h"

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int clntfd, bool one_shot){
    epoll_event event;
    event.data.fd = clntfd;
    event.events = EPOLLIN | EPOLLET;
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clntfd, &event);
    setnonblocking(clntfd);
}

void removefd(int epollfd, int clntfd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, clntfd, NULL);
    close(clntfd);
}

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

void http_conn::init(int clntfd, const sockaddr_in&addr){
    m_clntfd = clntfd;
    m_clntaddr = addr;
    addfd(m_epollfd, clntfd, true);
    m_user_count++;
    cgi = 0;
    query_string = NULL;
}

int http_conn::get_line(char* buf, int size){
    int i = 0 ;
    char c = '\0';
    int n;
    while((i<size-1)&&(c!='\n')){
        n = recv(m_clntfd, &c, 1, 0);
        if(n>0){
            if(c=='\r'){
                n = recv(m_clntfd, &c, 1, MSG_PEEK);
                if((n>0)&&(c=='\n'))recv(m_clntfd, &c, 1, 0);
                else c='\n';
            }
            buf[i] = c;
            i++;
        }
        else c='\n';
    }
    buf[i] = '\0';
    return (i);
}

void http_conn::process_read(){
    size_t i, j;
    i = 0;
    j = 0;
    char buf[1024];
    int numchars = get_line(buf, sizeof(buf));//返回request_line的长度

    while(!isspace(buf[i])&&(j<sizeof(m_method)-1)){
        m_method[j] = buf[i];
        i++;
        j++;
    }
    m_method[j] = '\0';

    //如果既不是POST请求也不是GET请求则返回501
    if(strcasecmp(m_method, "GET")&&strcasecmp(m_method, "POST")){//支持GET和POST请求
        not_implemented();
        // close_conn();
        return;
    }

    //POST请求
    if(strcasecmp(m_method, "POST")==0) cgi = 1;

    j = 0;
    //读完空格
    while(isspace(buf[i])&&(i<sizeof(buf)))i++;

    //读url
    while(!isspace(buf[i])&&(j<sizeof(m_url)-1)&&(i<sizeof(buf))){
        m_url[j] = buf[i];
        i++;
        j++;
    }
    m_url[j] = '\0';

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

    if(stat(m_path, &st) == -1){
        while((numchars>0)&&strcmp("\n", buf))numchars = get_line(buf, sizeof(buf));
        not_found();
        // close_conn();
        return ;
    }
    else{
        if((st.st_mode & S_IFMT) == S_IFDIR){//如果是目录,自动打开test.html
            printf("is directory");
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
    sprintf(m_write_buf, "HTTP/1.1 501 Method Not Implement\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/html\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</TITLE></HEAD>\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<BODY><P>HTTP request method not supported.\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</BODY></HTML>\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::not_found(){
    sprintf(m_write_buf, "HTTP/1.1 404 Not Found\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/html\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<HTML><TITLE>Not Found</TITLE>\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<BODY><P>The server could not fulfill\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "your request because the resource specified\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "is unavailable or nonexistent.\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "</BODY></HTML>\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::bad_request(){
    sprintf(m_write_buf, "HTTP/1.1 400 Bad Request\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/html\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "<P>Your browser sent a bad request, ");
    sprintf(m_write_buf+strlen(m_write_buf), "such as a POST without a Content-Length.\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::get_request(){
    sprintf(m_write_buf, "HTTP/1.1 200 OK\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/html\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "\r\n");
    send(m_clntfd, m_write_buf, strlen(m_write_buf), 0);
}

void http_conn::internal_error(){
    sprintf(m_write_buf, "HTTP/1.1 500 Internal Server Error\r\n");
    sprintf(m_write_buf+strlen(m_write_buf), "Content-Type: text/html\r\n");
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
    char buf[2048];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)){
        // printf("%s", buf);
        send(m_clntfd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void http_conn::execute_cgi(){
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    char content[1024];
    pid_t pid;
    int status;

    int i;
    char c;

    int numchars = 1;
    int content_length = -1;
    //默认字符
    buf[0] = 'A'; 
    buf[1] = '\0';
    if (strcasecmp(m_method, "GET") == 0)

        while ((numchars > 0) && strcmp("\n", buf))
        {
            numchars = get_line(buf, sizeof(buf));
        }
    else    
    {

        numchars = get_line(buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
                numchars = get_line(buf, sizeof(buf));
        }

        if (content_length == -1) {
        bad_request();
        return;
        }
    }


    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(m_clntfd, buf, strlen(buf), 0);
    if (pipe(cgi_output) < 0) {
        internal_error();
        return;
    }
    if (pipe(cgi_input) < 0) { 
        internal_error();
        return;
    }

    if ( (pid = fork()) < 0 ) {
        internal_error();
        return;
    }
    if (pid == 0)  /* 子进程: 运行CGI 脚本 */
    {
        char meth_env[300];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);


        close(cgi_output[0]);//关闭了cgi_output中的读通道
        close(cgi_input[1]);//关闭了cgi_input中的写通道

        
        sprintf(meth_env, "REQUEST_METHOD=%s", m_method);
        putenv(meth_env);

        if (strcasecmp(m_method, "GET") == 0) {
        //存储QUERY_STRING
        sprintf(query_env, "QUERY_STRING=%s", query_string);
        putenv(query_env);
        }
        else {   /* POST */
        //存储CONTENT_LENGTH
        sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
        putenv(length_env);
        }

        execl(m_path, m_path, NULL);//执行CGI脚本
        exit(0);
    } 
    else {  
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(m_method, "POST") == 0){
            for (i = 0; i < content_length; i++) 
            {
                recv(m_clntfd, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }

        //读取cgi脚本返回数据
        while (read(cgi_output[0], &c, 1) > 0)
            //发送给浏览器
        {			
            send(m_clntfd, &c, 1, 0);
        }

        //运行结束关闭
        close(cgi_output[0]);
        close(cgi_input[1]);

        waitpid(pid, &status, 0);
    }
}

void http_conn::close_conn(){
    if(m_clntfd!=-1){
        removefd(m_epollfd, m_clntfd);
        // m_clntfd = -1;
        m_user_count--;
    }
}

void http_conn::process(){
    process_read();
}
