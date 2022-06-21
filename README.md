# 						MyWebServer
## 1.简介
基于Linux实现的简单的webserver，应用层实现了Http服务器，采用线程池+EPOLL(ET模式)，支持GET和POST请求

## 2.编译

克隆项目并给予执行权限：

```c++
git clone  https://github.com/Crazybutcalm/MyWebServer.git
cd  httpdocs/
sudo chmod 600 test.html
sudo chmod 600 post.html
sudo chmod +X post.cgi
```

编译执行：

```
cd ../
make
./myweb
```

## 3.框架



参考资料：《TCP/IP网络编程》,《Linux高性能服务器编程》,以及开源项目https://github.com/forthespada/MyPoorWebServer

