# 						MyWebServer
## 1.Introduction
基于Linux实现的简单的webserver，应用层实现了Http服务器，支持GET和POST请求

实现get/post两种请求解析（是否加head）,采用cgi脚本进行post请求响应
利用多线程机制提供服务，增加并行服务数量，（线程池+epoll）
# 如何处理http请求，以及cgi脚本
# 线程池的编写
# 如何将epoll+线程池在不使用Reactor模式的前提下完成WebServer的编写

参考资料：《TCP/IP网络编程》，《Linux高性能服务器编程》