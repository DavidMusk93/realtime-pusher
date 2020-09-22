//
// Created by Steve on 8/24/2020.
//

#include "network.h"

Group::Group(std::string ip, short port):ip_(std::move(ip)),port_(port),fd_(-1){
    int fd;
    const int ON=1;
    ERROR_RETURN((fd=socket(AF_INET,SOCK_DGRAM,0))==-1,,,1);
    Cleaner cleaner{[&fd]{close(fd);}};
    SETSOCKOPT(fd,SOL_SOCKET,SO_REUSEADDR,ON,);
    SETSOCKOPT(fd,SOL_SOCKET,SO_REUSEPORT,ON,);
    MAKE_SOCKADDR_IN(local,INADDR_ANY,htons(port_));
    ERROR_RETURN(bind(fd,SOCKADDR_EX(local))==-1,,,1);
    LOG("(GROUP)join to %s:%d #%d",ip_.c_str(),port,fd);
    fd_=fd;
//    cleaner=Cleaner{[this]{FdHelper::Close(fd_);}};
    cleaner.cancel();
    ERROR_RETURN(Join(inet_addr(ip_.c_str()),INADDR_ANY)==false,,,0);
}

bool Group::Join(const in_addr_t group, const in_addr_t recv) {
    struct ip_mreq im{{group},{recv}};
    SETSOCKOPT(fd_,IPPROTO_IP,IP_ADD_MEMBERSHIP,im,false);
    return true;
}