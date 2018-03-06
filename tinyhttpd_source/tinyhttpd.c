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
/* 2018-3-4 开始注释，并完成main函数的注释。
 * 2018-3-5 完成核心函数startup，包括socket创建，绑定，监听；
            完成所有辅助函数。

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
/* HTTP协议规定，请求从客户端发出，最后服务器响应该请求并返回。
 * 这是目前HTTP协议的规定，服务器不支持主动响应，所以目前的HTTP
 * 协议版本都是基于客户端请求，然后响应的这种模型
*/
/* accept_request函数解析客户端请求，判断是请求静态文件还是cgi代码
 * （通过请求类型以及参数判断）。如果是静态文件则将文件输出给前端，
 * 如果是cgi则进入cgi处理函数。
*/
void accept_request(int client)
{
    char buf[1024];
    int numchars;
    char method[255];   //请求方法，GET或POST
    char url[255];  //请求的文件路径
    char path[512]; //文件的相对路径
    size_t i, j;
    struct stat st;     //stat结构体是用来描述一个linux系统文件中文件属性的结构
    int cgi = 0;    //cgi标志位，用于判断是否是动态请求

    char *query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf));  //从client中读取指定大小http数据到buf
    i = 0; j = 0;
    //接收客户端的http请求报文
    //接收字符处理：提取空格字符前的字符，至多254个
    while(!Isspace(buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[j];     //根据http请求报文格式，这里得到的是请求方法
        i++; j++;
    }
    method[i] = '\0';

    //忽略大小写比较字符串，判断使用的是那种请求方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);  //两种支持的方法都不是，告知客户端所请求的方法未能实现
        return;
    }

    if (strcasecmp(method, "POST") == 0)    //如果是POST方法
        cgi = 1;    //设置标志位，Post表示是动态请求

    i = 0;
    while (ISspace(buf[j]) && j < sizeof(buf))  //过滤掉空格字符
        j++;

    while (ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        url[i] = buf[j];    //得到URL(互联网标准资源的地址)
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)     //如果方法是get
    {
        query_string = url;     //请求信息
        while ((*query_string != '?') && (*query_string != '\0'))   //跳过？前面的字符
            query_string++; //问号前面是路径，后面是参数
        if (*query_string == '?')   //得到问号，表明是动态请求
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;     //此时指针指向问号的下一位
        }
    }

    //下面是项目中htdocs文件下的文件
    sprintf(path, "htdocs%s", url); //获取请求文件路径
    if (path[strlen(path) - 1 == '/')   //如果文件类型是目录(/)，则加上index.html
        strcat(path, "index.html");

    //根据路径找文件，并获取path文件信息保存到结构体st中，这就是函数stat的作用
    if (stat(path, &st) == -1)  //如果失败
    {
        //丢弃headers的信息
        while (numchars > 0 && strcmp("\n", buf))   //
            numchars = get_line(client, buf, sizeof(buf));  //
        not_found(client);  //回应客户端找不到
    }
    else    //如果文件信息获取成功
    {
        //如果是个目录，则默认使用该目录下index.html文件，stat结构体中的st_mode用于判断文件类型
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            cgi = 1;

        if (!cgi)   //静态页面请求
            serve_file(client, path);   //直接返回文件信息给客户端，静态页面返回
        else //动态页面请求
            execute_cgi(client, path, method, query_string);    //执行cgi脚本
    }

    close(client);  //关闭客户端socket
}

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
    //读取文件到buf中，fgets从文件结构体指针(FILE *resource)中读取数据，每次读取一行
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
/* Inform the client that a CGI script could not be executed
 * Parameter: the client socket descriptor.
*/
/*返回服务器错误，状态码500 */
void cannot_execute(int client)
{
    char buf[1024];
    //发送500，http状态码500，此错误是因为CGI脚本无法执行引起的
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/************** error_die ****************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error.
*/
/* 打印出错误信息，并终止 */
void error_die(const char *sc)
{
    perror(sc);     //perror函数用来将上一个函数发生错误的原因输出到标准设备(stderr)，
                    //参数sc所指字符串会先打印;此错误原因依照全局变量errno的值来决定要输出的字符串
                    //errno为记录系统的最后一次错误代码的全局变量
    exit(1);    //exit(1)表示异常退出
}

/************** execute_cgi ****************/
/* Execute a cgi script. Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
               path to the cgi script
*/
/* 执行cgi(公共网卡接口)脚本，需要设定合适的环境变量 */
/* execute_cgi函数负责将请求传递给cgi程序处理，
 * 服务器与cgi之间通过管道pipe通信，首先初始化两个管道，并创建子进程执行cgi函数
 * 子进程执行cgi程序，获取cgi的标准输出通过管道传给父进程，由父进程发送给客户端
*/
void execute_cgi(int client, const char *path const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)     //GET方法：一般用于获取/查询资源信息
        while ((numchars > 0) && strcmp("\n", buf)) //读取并丢弃头部信息
            numchars = get_line(client, buf, sizeof(buf));  //从客户端读取
    else    //POST方法，一般用于更新资源信息
    {
        numchars = get_line(client, buf, sizeof(buf));

        //获取HTTP消息实体的传输长度
        while ((numchars > 0) && strcmp("\n", buf)) //不空，且不为换行符
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content_Length:") == 0)    //是否为Content_Length字段
                content_length = atoi(&buf[16]);    //Content_Length用于描述HTTP消息实体的传输长度
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1)
        {
            bad_request(client);    //请求的页面数据为空，没有数据，就是我们打开网页经常出现的空白页面
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    //pipe函数建立管道，成功返回0，参数数组包含pipe使用的两个文件的描述符，fd[0]:读入端，fd[1]:写入端
    //必须在fork中调用pipe，否则子进程不会继承文件描述符。
    if (pipe(cgi_output) < 0)
    {
        cannot_execute(client); //管道建立失败
        return;
    }   //管道只能具有公共祖先的进程间进行，这里是父子进程之间
    if (pipe(cgi_input) < 0)
    {
        cannot_execute(client);
        return;
    }

    //fork子进程，这样创建了父子进程间的IPC(进程间通信)通道
    if ((pid = fork()) < 0)
    {
        cannot_execute(client);     //创建失败
        return;
    }

    /* 实现进程间的管道通信机制 */
    //子进程继承了父进程的pipe，然后通过关闭子进程output管道的out端，input管道的in端；
    //关闭父进程output管道的in端，input管道的out端
    //管道分为有名管道和无名管道，这里使用的是无名管道，
    //两个进程若不存在共享祖先进程则不能使用无名管道，这里是父子进程的关系
    //有名管道可以在一个系统中的任意两个进程之间通信
    //子进程
    if (pid == 0)   //这是子进程，用于执行cgi脚本程序
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        //复制文件句柄，重定向进程的标准输入输出
        //dup2函数在管道实现进程间通信中重定向文件描述符
        dup2(cgi_output[1], 1); // 1表示stdout，0表示stdin，将系统标准输出重定向为cgi_output[1]
        dup2(cgi_input[0], 0);  //将系统标准输入重定向为cgi_input[0]
        close(cgi_output[0]);   //关闭cgi_output中的out端
        close(cgi_input[1]);    //关闭cgi_input中的in端

        //cgi标准需要将请求的方法存储到环境变量中，然后和cgi脚本进行交互
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);       //putenv函数的作用是增加环境变量

        if (strcasecmp(method, "GET") == 0) //get
        {
            //设置query_string的环境变量
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else    //post
        {
            //设置content_Length的环境变量
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, paht, NULL);    //exec函数簇，执行cgi脚本，获取cgi的标准输出作为相应内容发送给客户端
        //因为通过dup2完成了重定向，标准输出内容进入管道output的输入端

        exit(0);    //子进程退出
    }
    else    //如果是父进程
    {
        close(cgi_output[1]);   //关闭cgi_output中的in通道，注意这里是父进程的cgi_output变量，和子进程区分开
        close(cgi_input[0]);    //关闭cgi_input的out通道
        /* 通过关闭对应管道的端口通道，然后重定向子进程的某端，这样就在父子进程之间构建了一条单双工通道
         * 如果不进行重定向，将是一条典型的全双工管道通信机制
        */
        if (strcasecmp(method, "POST") == 0)    //post方式，将指定好的传输长度字符发送
            /* 接收post过来的数据 */
        for (i = 0; i < content_length; i++)
        {
            recv(client, &c, 1, 0); //从客户端接收单个字符
            write(cgi_input[1], &c, 1); //写入cgi_input的in通道
            //数据传送过程：input[1](parent) --> input[0](child)[执行cgi函数] --> STDIN --> STDOUT
            // --> output[1](child) -- > output[0](parent)[将结果发送给客户端]

        }
        while (read(cgi_output[0], &c, 1) > 0)  //读取输出内容到客户端
            send(client, &c, 1, 0); //

        close(cgi_output[0]);   //关闭剩下的管道端
        close(cgi_input[1]);
        waitpid(pid, &status, 0);   //等待子进程终止
    }

}


/************** get_line ****************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination. Terminates the string read
 * with a null character. If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null. If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null)
*/
/* 读取一行http报文，以\r或\r\n结尾 */
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';  //'\0'表示NULL
    int n;

    //至多读取size-1个字符，最后一个字符置为'\0'
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);   //单个字符接收
                                    //客户和服务器都用recv函数从TCP连接的另一端接收数据
                                    //第一个参数指定接收端套接字描述符；
                                    //第二个参数指明一个缓冲区地址，该缓冲区用来存放recv函数接收到的数据

        if (n > 0)  //recv接收成功
        {
            if (c == '\r')  //如果是回车符，则继续读取
            {
                //使用MSG_PEEK标志使下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动
                n = recv(sock, &c, 1, MSG_PEEK);

                if ((n > 0) && (c == '\n')) //  如果是回车换行符
                    recv(sock, &c, 1, 0);   //继续接收单个字符，实际上和上面那个标志位MSG_PEEK读取同样的字符
                                            //读完删除输入队列的数据，即滑动窗口，c=='\n'
                else
                    c = '\n';   //只是读取到回车符，则值为换行符，也终止了读取
            }
            buf[i] = c; //将读取到的数据放入缓冲区
            i++;    //循环进行下一次读取
        }
        else    //没有读到任何数据
            c = '\n';
    }
    buf[i] = '\0';

    return(i);  //返回读取到的字符个数（包括'\0'）
}

/************** headers ****************/
/* Return the informational HTTP headers about a file.
 * Parameters: the socket to print the headers on the
 * name of the file
*/
/* 成功返回http响应头部信息 */
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename; //(void)var 的作用是：避免未使用变量的编译警告

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);         //SERVER_STRING为define的server名字
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/************** not_found ****************/
/* Give a client a 404 not found status message
*/
/* 返回404 */
void not_found(int client)
{
    char buf[1024];
    //返回404
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Conent-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(cliend, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}


/************** serve_file ****************/
/* Send a regular file to the client. Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *             file descriptor
 *             the name of the file to serve
*/
/* 将请求的文件发送回client */
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';  //这两个不知道是干什么的
    while ((numchars > 0) && strcmp("\n", buf))     //read & discard headers
        numchars = get_line(client, buf, sizeof(buf));      //按行读取值client

    resource = fopen(filename, "r");    //以只读形式打开文件
                                        //fopen打开文件，并返回文件指针（FILE*）
    if (resource == NULL)
        not_found(client);  //如果文件不存在，调用not_found返回404
    else
    {
        headers(client, filename);  //先返回文件头部信息，即正常读取文件200
        cat(client, resource);      //将resource描述符指定文件中的数据发送给client
    }
    fclose(resource);   //关闭
}

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
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket
*/
/* 通知client所提出的请求方法不被服务器支持 */
void unimplemented(int client)
{
    char buf[1024];
    //发送501表示相应方法未实现
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(cliend, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

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
