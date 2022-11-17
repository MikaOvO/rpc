#pragma once 

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.hpp"

int set_non_blocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void add_fd(int epollfd, int fd, bool one_shot, triger trig_mode) {
    epoll_event event;
    event.data.fd = fd;
    if (triger::ET_triger == trig_mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_blocking(fd);
}

void remove_fd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void mod_fd(int epollfd, int fd, int ev, bool one_shot, triger trig_mode) {
    epoll_event event;
    event.data.fd = fd;
    if (triger::ET_triger == trig_mode)
        event.events = ev | EPOLLET | EPOLLRDHUP;
    else
        event.events = ev | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
