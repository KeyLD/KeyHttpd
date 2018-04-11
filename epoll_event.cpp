/* ***********************************************************************
    > File Name: epoll_event.cpp
    > Author: Key
    > Mail: keyld777@gmail.com
    > Created Time: Tue 10 Apr 2018 10:34:52 PM CST
*********************************************************************** */

#include "epoll_event.h"

void WebEventPoll::addEvent(web_connection_t* conn, int events)
{
    epoll_event ee;
    int fd = conn->fd;
    ee.events = events;
    ee.data.ptr = (void*)conn;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee);
}

void WebEventPoll::modEvent(web_connection_t* conn, int events)
{
    epoll_event ee;
    int fd = conn->fd;
    ee.events = events;
    ee.data.ptr = (void*)conn;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ee);
}

void WebEventPoll::delEvent(web_connection_t* conn)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, 0);
}

void WebEventPoll::epollHandle()
{
    epoll_event events[MAX_EVENT_NUMBER];
    while (true) {
        int active_fds_count = epoll_wait(epfd, events, MAX_EVENT_NUMBER, -1);
        for (int i = 0; i < active_fds_count; i++) {
            epoll_event& event = events[i];
            web_connection_t* conn = (web_connection_t*)(event.data.ptr);
            if(event.events & EPOLLIN) {
                conn->read_event->handler(conn);

            } else if(event.events & EPOLLOUT) {
                conn->write_event->handler(conn);

            } else if(event.events & EPOLLERR) {

            }
        }
    }
}
