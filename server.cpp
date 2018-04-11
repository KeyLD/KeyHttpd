/* ***********************************************************************
    > File Name: server.cpp
    > Author: Key
    > Mail: keyld777@gmail.com
    > Created Time: Tue 10 Apr 2018 10:33:09 PM CST
*********************************************************************** */

#include "server.h"
#include "epoll_event.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
using namespace std;

WebEventPoll wep;

int main()
{
    signal(SIGPIPE, SIG_IGN); //原因：http://blog.sina.com.cn/s/blog_502d765f0100kopn.html

    int ss_fd = startUp();
    web_connection_t* conn = new web_connection_t;

    conn->fd = ss_fd;
    conn->read_event->handler = web_accept;
    conn->write_event->handler = empty_event_handler;
    wep.addEvent(conn, EPOLLIN | EPOLLERR);

    setnonblocking(ss_fd);

    wep.epollHandle();
}

void errorQuit(const char* msg)
{
    perror(msg);
    exit(1);
}

int startUp()
{
    int sock_fd, reuse = 1;
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof server_addr);

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorQuit("socket error");

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        // 解决time wait引起的bind error 建议非调试阶段删除
        errorQuit("setsockopt error");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (SA*)&server_addr, sizeof server_addr) < 0)
        errorQuit("bind error");

    if (listen(sock_fd, 50) < 0)
        errorQuit("listen error");

    return sock_fd;
}

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void web_accept(web_connection_t* conn)
{
    struct sockaddr_in client_addr;
    socklen_t client_addrlength = sizeof(client_addr);
    int connfd;
    if ((connfd = accept(conn->fd, (SA*)&client_addr, &client_addrlength)) < 0) {
        perror("accept error");
        return;
    }
    char ip[20];
    printf("Accept new connection from %s : %d\n", inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof ip), ntohs(client_addr.sin_port));
    web_connection_t* new_conn = new web_connection_t;

    new_conn->fd = connfd;
    new_conn->state = READ;
    new_conn->read_event->handler = read_request;

    wep.addEvent(new_conn, EPOLLIN | EPOLLERR | EPOLLET);

    setnonblocking(connfd);
}

void read_request(web_connection_t* conn)
{
    int client_fd = conn->fd;
    char buf[MAXLINE];

    if (getLine(client_fd, buf, MAXLINE) == 0) {
        wep.delEvent(conn);
        close_conn(conn);
        return;
    }
    sscanf(buf, "%s %s %s", conn->method, conn->uri, conn->version);

    if (strcasecmp(conn->method, "GET") && strcasecmp(conn->method, "POST")) {
        clientError(client_fd, conn->method, "501", "Not implemented",
            "httpd does not implement this method");
        return;
    }
    readHeadOthers(conn, client_fd);

    bool is_dyn = parseUri(conn);

    conn->state = SEND_DATA;
    conn->write_event->handler = is_dyn ? dynamicResponse : staticResponse;
    conn->read_event->handler = empty_event_handler; //读事件回调函数设置为空
    wep.modEvent(conn, EPOLLOUT | EPOLLERR | EPOLLET);
}

bool parseUri(web_connection_t* conn)
{
    bool cgi = false;
    char* query_string = nullptr;
    if (strcasecmp(conn->method, "GET") == 0) {
        query_string = conn->uri;
        while ((*query_string != '?') and (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {
            cgi = true;
            *query_string = '\0';
            query_string++;
        }
    } else
        cgi = true;
    conn->query_string = query_string;

    char path[MAXLINE];
    struct stat* sbuf = (struct stat*)malloc(sizeof(struct stat));
    sprintf(path, "htdocs%s", conn->uri); // hit
    if (path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
        sprintf(conn->uri, "%sindex.html", conn->uri);
    }

    if (stat(path, sbuf) < 0) {
        clientError(conn->fd, conn->method, "404", "Not found",
            "Httpd couldn't find the file");
    } else {
        if ((sbuf->st_mode & S_IFMT) == S_IFDIR)
            strcat(conn->uri, "/index.html");
        if ((sbuf->st_mode & S_IXUSR) || (sbuf->st_mode & S_IXGRP) || (sbuf->st_mode & S_IXOTH))
            //如果这个文件是一个可执行文件，不论是属于用户/组/其他这三者类型的，就将 cgi 标志变量置一
            cgi = true;
    }
    return cgi;
}

void readHeadOthers(web_connection_t* conn, int client_fd)
{
    printf("%s \n", conn->method);
    char left[MAXLINE];
    char buf[MAXLINE];
    while (strcmp(buf, "\r\n")) {
        getLine(client_fd, buf, MAXLINE);
        sscanf(buf, "%[^:]", left);
        if (strcasecmp(left, "Host") == 0) {
            strcpy(conn->host, buf + 4);
            printf("%s", buf);
        } else if (strcasecmp(left, "Connection") == 0) {
            strcpy(conn->connection, buf + 10);
            printf("%s", buf);
        } else if (strcasecmp(left, "Accept") == 0) {
            strcpy(conn->accept, buf + 6);
            printf("%s", buf);
        }
    }
}

void staticResponse(web_connection_t* conn)
{
    char path[128];
    sprintf(path, "htdocs%s", conn->uri);

    int filefd = open(path, O_RDONLY);
    if (filefd < 0) {
        clientError(conn->fd, path, "404", "Not found",
            "Httpd couldn't find the file");
        return;
    }

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    char filetype[50], buf[MAX_BUF];

    getFileType(conn->uri, filetype);

    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Key Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf); //关闭连接 否则会一直加载  别问我为什么
    sprintf(buf, "%sContent-length: %ld\r\n", buf, stat_buf.st_size);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    write(conn->fd, buf, strlen(buf));

    printf("\nResponse headers:\n%s\n", buf);

    sendfile(conn->fd, filefd, NULL, stat_buf.st_size);
    close(filefd);

    conn->state = READ;
    conn->read_event->handler = read_request;
    wep.modEvent(conn, EPOLLIN | EPOLLERR | EPOLLET);
}

void dynamicResponse(web_connection_t* conn)
{
    char path[MAXLINE], buf[MAXLINE * 10] = "Key", filetype[30];
    char argv[MAXLINE];
    int f2s[2], s2f[2];
    pid_t pid;
    int status;

    sprintf(path, "htdocs%s", conn->uri);
    getFileType(conn->uri, filetype);

    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Key Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf); //关闭连接 否则会一直加载  别问我为什么
    write(conn->fd, buf, strlen(buf));

    if (strcasecmp(conn->method, "GET") == 0) {
        int num = 1;
        while (num > 0 and strcmp(buf, "\n"))
            num = getLine(conn->fd, buf, MAXLINE);
    } else {
        int num = getLine(conn->fd, buf, MAXLINE);
        while (num > 0 and strcmp(buf, "\n")) {
            strcat(argv, buf);
            strcat(argv, "&");
            num = getLine(conn->fd, buf, MAXLINE);
        }
        int len = strlen(argv);
        if (len > 0)
            argv[strlen(argv) - 1] = '\0';
        conn->query_string = argv;
    }

    if (pipe(f2s) < 0) {
        clientError(conn->fd, conn->method, "500", "Internal Server Error",
            "Error prohibited CGI execution.");
        return;
    }
    if (pipe(s2f) < 0) {
        clientError(conn->fd, conn->method, "500", "Internal Server Error",
            "Error prohibited CGI execution.");
        return;
    }
    if ((pid = fork()) < 0) {
        clientError(conn->fd, conn->method, "500", "Internal Server Error",
            "Error prohibited CGI execution.");
        return;
    }
    if (pid == 0) { // child
        close(s2f[0]);
        close(f2s[1]);
        dup2(s2f[1], STDOUT_FILENO);
        dup2(f2s[0], STDIN_FILENO);

        //dup2(conn->fd, STDOUT_FILENO);
        char env[MAXLINE];
        sprintf(env, "REQUEST_METHOD=%s", conn->method);
        putenv(env);

        sprintf(env, "QUERY_STRING=%s", conn->query_string);
        putenv(env);

        execl("/bin/ruby", "ruby", path, conn->query_string, 0);
        exit(0);
    } else {
        close(s2f[1]);
        close(f2s[0]);
        sleep(2);

        read(s2f[0], buf, MAXLINE);
        write(conn->fd, buf, MAXLINE);

        close(s2f[0]);
        close(f2s[1]);
        waitpid(pid, &status, 0);
    }
    close(conn->fd);
    //不关闭就会一直加载，很奇怪阿，服务端关闭客户端套接字描述符会发生什么，即使关闭了之后还能继续使用……

    conn->state = READ;
    conn->read_event->handler = read_request;
    conn->write_event->handler = empty_event_handler;
    wep.modEvent(conn, EPOLLIN | EPOLLERR | EPOLLET);
}

void empty_event_handler(web_connection_t* conn)
{
    puts("Handle error");
    exit(1);
}

void close_conn(web_connection_t* conn)
{
    static int count = 0;
    count++;
    printf("关闭第%d个连接\n", count);
    close(conn->fd);
    delete conn;
}

void getFileType(char* filename, char* filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".ico"))
        strcpy(filetype, "image/icon");
    else
        strcpy(filetype, "text/plain");
}

int getLine(int sock, char* buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                buf[i++] = c;
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i++] = c;
        } else
            c = '\n';
    }
    buf[i] = '\0';
    return i;
}

void clientError(int fd, char* cause, const char* errnum,
    const char* shortmsg, const char* longmsg)
{
    char buf[MAXLINE], body[MAX_BUF];
    sprintf(body, "<html><center><title>Httpd Error</title>");
    sprintf(body, "%s<body bgcolor="
                  "ffffff"
                  ">\r\n",
        body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Key Httpd</em></center>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
    write(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    write(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    write(fd, buf, strlen(buf));

    write(fd, body, strlen(body));
}
