#include <iostream>
#include <pthread.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include<fstream>
// #include <ctype.h>
// #include <sys/stat.h>
// #include <sys/wait.h>
// #include <stdlib.h>
using namespace std;
int start_up(unsigned int&);
void* accept_request(void *);
int get_line(int, string&, int);
void bad_request(int);
void cat(int, fstream&);
void cannot_execute(int);
void error_die(const char *);
void execute_gui(int, const char *, const char *, const char *);

void headers(int, const char *);
void not_found(int);
void unimplemented(int);
void serve_file(int, const char *);

//处理监听到的http请求
void* accept_request(void *client){
    int clnt_sock = *(int *)client;
    string buf;
    string method;
    get_line(clnt_sock, buf);//读取请求头到buf中

    //处理请求方式
    int i = 0;
    while(i<buf.size()&&buf[i]!=' '){
        method += buf[i];
        i++;
    }
    //既不是get请求也不是post请求，一般来说get请求是访问，post请求是修改
    if(method.compare("GET")!=0&&method.compare("POST")!=0){
        unimplemented(clnt_sock);
        return NULL;
    }
}
void get_line(int clnt_sock, string& buf){
    int i=0;
    char c = '\0';
    int n;
    while(c!='\n'){
        int n = recv(clnt_sock, &c, 1, 0);
        if(n>0){
            if(c=='\r'){
                n=recv(clnt_sock, &c, 1, MSG_PEEK);
                if(n>0&&c=='\n')recv(clnt_sock, &c, 1, 0);
                else c = '\n';
            }
            buf+=c;
            i++;
        }
        else c = '\n';
        i++;
    }
    buf+='\0';
    return ;
}

int start_up(unsigned int &port){
    int httpd;
    socklen_t sock_len;
    sockaddr_in http_addr;
    httpd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)); //端口复用
    if(httpd==-1){

    }
    memset(&http_addr, 0, sizeof(http_addr));
    http_addr.sin_family = AF_INET;
    http_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    http_addr.sin_port = htons(port);

    if(bind(httpd, (sockaddr*) &http_addr, sizeof(http_addr)==-1)){
        
    }

    if(listen(httpd, 15)==-1){

    }//listen第二个参数是半连接队列的长度

    return httpd;
}

//传入客户端fd，服务端无法解析请求方法，返回码501
void unimplemented(int clnt_sock){
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(clnt_sock, buf, sizeof(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt_sock, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt_sock, buf, sizeof(buf), 0);

    //HTML的消息头和消息体
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(clnt_sock, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(clnt_sock, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(clnt_sock, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(clnt_sock, buf, strlen(buf), 0);
}
int main(){
    int serv_sock;
    int clnt_sock;
    unsigned int port =  9190;
    serv_sock = start_up(port);
    sockaddr_in clnt_addr;
    socklen_t clnt_len;
    pthread_t newthread;
    clnt_len = sizeof(clnt_addr);
    while(1){
        clnt_sock = accept(serv_sock, (sockaddr *) &clnt_addr, &clnt_len);
        cout<<"New connection....  ip:"<<inet_ntoa(clnt_addr.sin_addr)<<"port:"<<ntohs(clnt_addr.sin_port)<<endl;
        if(clnt_sock == -1){

        }
        if(pthread_create(&newthread, NULL, accept_request, &clnt_sock)==-1){

        }//需要是函数指针
    }
    close(serv_sock);
    return 0;
}
