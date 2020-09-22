//
// Created by Steve on 8/17/2020.
//

#include <utility>

#include "network.h"

int Connection::fd() const {
    return fd_;
}

int Connection::epollRegister(int epoll_handler, int events) const {
    struct epoll_event ev{};
    ev.events=events;
    ev.data.fd=fd_;
    ERROR_RETURN(epoll_ctl(epoll_handler,EPOLL_CTL_ADD,fd_,&ev)==-1,-1,,1);
    return 0;
}

int Connection::epollUnregister(int epoll_handler) {
    struct epoll_event ev{};
    ev.data.fd=fd_;
    ERROR_RETURN(epoll_ctl(epoll_handler,EPOLL_CTL_DEL,fd_,&ev)==-1,-1,,1);
//    Cleanup(fd_); /*this API should not manipulate the FD*/
    return 0;
}

void Connection::registerCallback(Callback callback, void *user_data) {
    callback_=std::move(callback);
    user_data_=user_data;
}

void Connection::eventTrigger(int epoll_handler,int events) {
    if(events&EPOLLRDHUP){
        epollUnregister(epoll_handler);
        if(callback_.on_close){
            callback_.on_close(fd_);
        }
    }else if(events&EPOLLIN){
        if(callback_.on_recv){
            callback_.on_recv(fd_,user_data_);
        }
    }
}