/* ***********************************************************************
    > File Name: epoll_event.h
    > Author: Key
    > Mail: keyld777@gmail.com
    > Created Time: Tue 10 Apr 2018 10:33:59 PM CST
*********************************************************************** */
#ifndef _EPOLL_EVENT_H
#define _EPOLL_EVENT_H

#include "server.h"
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

class WebEventPoll {
    int epfd;

public:
    WebEventPoll(int size = 1024) { epfd = epoll_create(size); }
    ~WebEventPoll() { close(epfd); }
    void addEvent(web_connection_t*, int);
    void modEvent(web_connection_t*, int);
    void delEvent(web_connection_t*);
    void epollHandle();
};
#endif
