/* ***********************************************************************
    > File Name: server.h
    > Author: Key
    > Mail: keyld777@gmail.com
    > Created Time: Tue 10 Apr 2018 10:32:08 PM CST
*********************************************************************** */
#ifndef _SERVER_H
#define _SERVER_H

#include <stdlib.h>

#define MAX_EVENT_NUMBER 10000
#define QUERY_INIT_LEN 1024
#define REMAIN_BUFFER 512
#define MAXLINE 1024
#define MAX_BUF 102400

/* 以下是处理机的状态 */
#define ACCEPT 1
#define READ 2
#define QUERY_LINE 4
#define QUERY_HEAD 8
#define QUERY_BODY 16
#define SEND_DATA 32

const int server_port = 8080;
typedef struct sockaddr SA;

class web_connection_t;

typedef void (*event_handler_pt)(web_connection_t* conn);

//每一个事件都由web_event_t结构体来表示
class web_event_t {
public:
    static int epfd;
    /*为1时表示事件是可写的，通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态*/
    unsigned write : 1;
    /*为1时表示此事件可以建立新的连接,通常情况下，在ngx_cycle_t中的listening动态数组中，每一个监听对象ngx_listening_t对应的读事件中 
    的accept标志位才会是1*/
    unsigned accept : 1;
    //为1时表示当前事件是活跃的，这个状态对应着事件驱动模块处理方式的不同，例如：在添加事件、删除事件和处理事件时，该标志位的不同都会对应着不同的处理方式
    unsigned active : 1;
    unsigned oneshot : 1;
    unsigned eof : 1;   //为1时表示当前处理的字符流已经结束
    unsigned error : 1; //为1时表示事件处理过程中出现了错误

    unsigned closed : 1;      //为1时表示当前事件已经关闭
    event_handler_pt handler; //事件处理方法，每个消费者模块都是重新实现它
};

class web_connection_t {
public:
    int fd;

    int state; //当前处理到哪个阶段
    web_event_t* read_event;
    web_event_t* write_event;
    char* query_string;

    char method[8];
    char uri[128];
    char version[16];
    char host[128];
    char accept[128];
    char connection[20];

    web_connection_t()
    {
        read_event = (web_event_t*)malloc(sizeof(web_event_t));
        write_event = (web_event_t*)malloc(sizeof(web_event_t));
        state = ACCEPT;
    }
    ~web_connection_t()
    {
        free(read_event);
        free(write_event);
    }
};

int setnonblocking(int fd);
void web_accept(web_connection_t* conn);
void read_request(web_connection_t* conn);
void process_request_line(web_connection_t* conn);
void process_head(web_connection_t* conn);
void process_body(web_connection_t* conn);
void staticResponse(web_connection_t* conn);
void dynamicResponse(web_connection_t* conn);
void empty_event_handler(web_connection_t* conn);
void close_conn(web_connection_t* conn);
int startUp();
void errorQuit(const char*);
void clientError(int, char*, const char*, const char*, const char*);
void readHeadOthers(web_connection_t*, int);
bool parseUri(web_connection_t*);
void getFileType(char*,char*);
int getLine(int, char*, int);

#endif
