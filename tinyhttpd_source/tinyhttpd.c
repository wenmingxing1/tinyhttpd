/* tinyhttpd webserver */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 * 1) Comment out the #include<pthread.h> line.
 * 2) Comment out the line that defines the variable newthread
 * 3) Comment out the two lines that run pthread_create()
 * 4) Uncomment the line that runs accept_request()
 * 5) Remove -lsocket from the Makefile.
*/

/* logs */
/* 2018-3-4 开始注释，并完成main函数的注释
 * 2018-3-5
*/

#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<strings.h>
#include<string.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/wait.h>
#include<stdlib.h>

#define ISspace(x) isspace((int)(x))
//函数说明：检查参数c是否为空格字符
//也就是判断是否为空格(' ')，定位字符('\t')，CR('\r'),换行('\n')等情况
//返回值：若c为空白字符，则返回非0，否则返回0

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"      //定义server名称

void accept_request(int);   //处理从套接字上监听到的一个http请求
void bad_request(int);  //返回给客户端这是个错误请求，400响应码
void cat(int, FILE *);  //读取服务器上某个文件(FILE) 写到socket套接字(int)
void cannot_execute(int);   //处理发生在执行cgi程序时出现的错误，cgi为通用网关接口技术，可以让一个客户端，从网页浏览器向执行在服务器上的程序请求数据
void error_die(const char *);   //将错误信息写到perror
void execute_cgi(int, const char *, const char *, const char *);    //运行cgi脚本，涉及到动态解析
int get_line(int, char *, int); //读取一行http报文
void headers(int, const char *);    //返回http相应头
void not_found(int);    //返回找不到请求文件
void serve_file(int, const char *); //调用cat，将服务器文件内容返回给浏览器
int startup(u_short *); //开启http服务，包括绑定端口，监听，开启线程处理链接
void unimplemented(int);    //返回给浏览器表明收到的http请求所用的method不被支持


int main()
{
    int server_sock = -1;
    u_short port = 0;   //传入的端口为0
    int client_sock = -1;
    struct sockaddr_in client_name;      //sockaddr_in数据结构用作网络编程函数的参数，指明地址信息。
    int client_name_len = sizeof(client_name);
    pthread_t newthread;    //pthread_t用于声明线程ID

    server_sock = startup(&port);   //服务器端监听套接字设置
    printf("httpd running on port %d\n", port);

    /*多线程并发服务器模式*/
    while(1)
    {
        /*主线程*/
        //accept函数等待来自客户端的连接请求到达侦听描述符server_sock，第二个参数为填入客户端套接字地址(&client_name)
        //返回一个已连接的描述符，若成功则为非负连接描述符，若出错则为-1
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);   //accept阻塞等待客户端连接请求

        if (client_sock == -1)      //accept未成功返回已连接描述符
            error_die("accept");    //error_die函数将错误信息写到perror中。

        /*派生新新线程用accept_request函数处理新请求*/
        /*accept_request(client_sock);*/
        //pthread_create为类Unix系统中创建线程的函数，成功则返回0，client_sock为向线程函数accept_request传递的参数
        if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0) //创建工作线程，执行回调函数accept_request，参数client_sock
            perror("pthread_create");      //perror打印最近一次系统错误信息，此处为创建线程失败
    }

    close(server_sock); //关闭套接字，就协议栈而言，即关闭TCP连接

    return 0;
}
