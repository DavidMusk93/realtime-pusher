//
// Created by Steve on 8/17/2020.
//

#ifndef C4FUN_NETWORK_H
#define C4FUN_NETWORK_H

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/tcp.h> /*for TCP_QUICKACK & TCP_NODELAY*/

//#include "time/now.h"
#include "tool.h"

#define WHERE_FALSE while(0)

#define ERROR_S strerror(errno)
#define ERROR_RETURN(expr,code,cleanup,err) \
do{\
    if(expr){\
        cleanup\
        LOG("%s%s",#expr,err?(": "+std::string(ERROR_S)).c_str():"");\
        return code;\
    }\
}WHERE_FALSE

#define SETSOCKOPT(sock,level,name,val,code) \
ERROR_RETURN(setsockopt(sock,level,name,&val,sizeof(val))!=0,code,{close(sock);sock=-1;},true)

#define SOCKADDR_EX(x) (struct sockaddr*)&x,sizeof(x)
#define SOCKADDR_FMT "%s:%d"
#define SOCKADDR_OF(x) inet_ntoa((x).sin_addr),ntohs((x).sin_port)

#define MAKE_SOCKADDR_IN(name,addr,port) \
struct sockaddr_in name{};\
name.sin_family=AF_INET;\
name.sin_addr.s_addr=addr;\
name.sin_port=port

class Connection{
public:
    using on_recv_t=std::function<void(int,void*)>;
    using on_send_t=std::function<void(int,void*)>;
    using on_close_t=std::function<void(int&)>;
    struct Callback{
        on_recv_t on_recv;
        on_send_t on_send;
        on_close_t on_close;
    };
//    using Callback=std::function<void(int,void*)>;
    using Handler=std::shared_ptr<Connection>;
    using Ptr=Handler;
    using Set=std::unordered_set<Ptr>;
    Connection():Connection(-1){}
    Connection(int fd):fd_(fd){}
    Connection&operator=(Connection&&other)noexcept{
        std::swap(fd_,other.fd_);
        std::swap(callback_,other.callback_);
        std::swap(user_data_,other.user_data_);
        return *this;
    }
    ~Connection(){
        Cleanup(fd_);
    }
    int fd() const;
    int epollRegister(int epoll_handler,int events) const;
    int epollUnregister(int epoll_handler);
    void registerCallback(Callback callback,void*user_data);

#define REGISTER_CALLBACK(x) \
callback_.x.swap(x); \
return this;
    Connection*registerOnRecv(on_recv_t on_recv,void*user_data=nullptr){
        user_data_=user_data;
        REGISTER_CALLBACK(on_recv);
    }
    Connection*registerOnSend(on_send_t on_send){
        REGISTER_CALLBACK(on_send);
    }
    Connection*registerOnClose(on_close_t on_close){
        REGISTER_CALLBACK(on_close);
    }
    void eventTrigger(int epoll_handler,int events);
    void cancel(){
        fd_=-1;
    }

    static void Cleanup(int&fd){
        if(fd!=-1){
            close(fd);
            fd=-1;
        }
    }

    std::string&Buffer(){
        return buffer_;
    }

    size_t&ReadBytes(){
        return read_bytes_;
    }

private:
    int fd_;
    bool legal_{false};
    Callback callback_;
    void*user_data_;
    std::string buffer_;
    size_t read_bytes_{0};
};

class Server{
public:
    enum class Type:char{
        TCP,
        UDP,
    };
    struct Config{
        const char*ip;
        short port;
        int backlog;
    };
    Server(Type type,Config config,Terminator&terminator);
    ~Server();

protected:
    void run();

    static void Accept(int fd,void*user_data);

private:
    Connection::Handler server_handler_;
    std::thread actor_;
    Terminator&terminator_;
    std::unordered_set<Connection::Handler> connections_;
    Connection::Handler new_connection_;
};

class Group{
public:
    Group(std::string ip,short port);
//    ~Group(){
//        FdHelper::Close(fd_);
//    }
    [[nodiscard]] int fd() const{
        return fd_;
    }

protected:
    bool Join(in_addr_t group,in_addr_t recv);

private:
    std::string ip_;
    short port_;
    int fd_; //shared_ptr may be better
};

#endif //C4FUN_NETWORK_H
