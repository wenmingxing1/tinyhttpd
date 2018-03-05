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

/************** accept_request ****************/
/* A request has caused a call to accept() on the server port to
 * return. Process the request appropriately.
 * Parameters: the socket connected to the client
*/

/************** bad_request ****************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket
*/
/* 返回给客户端这时错误请求，400响应码 */
void bad_request(int client)
{
    char buf[1024];
    //发送400，及相关信息
    sprintf(buf, "HTTP/1.0 400 BAD REWUEST\r\n");   //sprintf将格式化的数据写入字符串
    send(client, buf, sizeof(buf), 0);      //将buf发送给client
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Cotent-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/************** cat ****************/
/* Put the entire contents of a file out on a socket. This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
                FILE pointer for the file to cat
*/
/* 将文件结构指针resource中的数据发送至client */
void cat(int client, FILE *resource)
{
    //发送文件的内容
    char buf[1024];
    //读取文件到buf中
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource)) //判断文件是否读取到末尾；feof函数检测流上的文件结束符，如文件结束返回非0值
    {
        //将文件流中的字符全部发送给client
        send(client, buf, strlen(buf), 0);        //客户和服务器都是以send函数向TCP连接的另一端发送数据
                                                  //第一个参数为发送端套接字，第四个参数一般为0

        fgets(buf, sizeof(buf), resource);

        /* 从文件结构体指针resource中读取至多bufsize-1个数据（'\0'）
         * 每次读取一行，如果不足bufsize，则读取完该行结束。
         * 通过feof函数判断fgets是否因出错而终止
         * 另外，这里有文件偏移位置，下一轮读取会从上一轮读取完的位置继续
        */
    }
}

/************** cannot_execute ****************/
/*

*/

/************** error_die ****************/
/*

*/

/************** execute_cgi ****************/
/*

*/

/************** get_line ****************/
/*

*/

/************** headers ****************/
/*

*/

/************** not_found ****************/
/*

*/

/************** serve_file ****************/
/*

*/

/************** startup ****************/
/* This function starts the process of listening for web connections
 * on a specified port. If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket
*/
/*启动服务器，包括创建服务器套接字，绑定端口，监听；
 *返回一个监听套接字
*/
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);    //创建服务器端套接字，成功但会非负，失败返回-1
                                                //在代码中几乎都是本句的写法，这里注意PF_INET与AF_INET的区别（其实是可以混用的，是指不规范）
                                                //AF_INET表明我们正在使用因特网
                                                //SOCK_STREAM表示这个套接字是因特网连接的一个端点
    if (httpd == -1)
        error_die("socket");

    memset(&name, 0, sizeof(name));
    /*设置套接字地址结构*/
    name.sin_family = AF_INET;  //设置地址簇，sin_family表示address family，AF_INET表示使用TCP/IP协议族的地址
    name.sin_port = htons(*port);   //指定端口，sin_port表示port number，htons函数将主机的无符号短整型数转换成网络字节顺序
    name.sin_addr.s_addr = htonl(INADDR_ANY);   //通配地址，s.addr表示ip address，htonl将主机数转换成无符号长整型的网络字节顺序

    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)    //将httpd套接字绑定到指定地址和端口
                                                                    //bind函数将name中的服务器套接字地址和套接字描述符httpd联系起来
                                                                    //成功返回0，失败返回-1
        error_die("bind");

    if (*port == 0) //如果*port==0，则动态分配一个端口
    {
        int namelen = sizeof(name);
        //在以端口0调用bind后，getsockname用于返回由内核赋予的本地端口号
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)   //getsockname用于获取套接字的地址结构
                                                                            //在这里是以端口号0调用bind之后，用于返回内核赋予的本地端口号
                                                                            //成功返回0，失败返回-1
            error_die("getsockname");
        *port = ntohs(name.sin_port);   //网络字节顺序转换为主机字节顺序，返回主机字节顺序表达的数
    }

    if (listen(httpd, 5) < 0)   //listen将httpd将主动套接字转化为监听套接字，之后可以接受来自客户端的连接请求
                                //5这个参数暗示了内核在开始拒绝连接请求之前，应该放入队列中等待的未完成连接请求的数量，通常设置为1024
        error_die("listen");

    return(httpd);
}

/************* unimplemented **************/
/*

*/

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
