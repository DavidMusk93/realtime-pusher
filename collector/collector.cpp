//
// Created by Steve on 8/24/2020.
//

#include "collector.h"
#include "codec.h"
#include "secure.h"
#include "logger.h"

#define _CALL_TAG(tag) LOG(TAG "@%s call " #tag,__func__)
#define CALL_START() _CALL_TAG(START)
#define CALL_END()   _CALL_TAG(END)

void Collector::HandleGroupMsg(int fd, void *user_data){
    auto collector=reinterpret_cast<Collector*>(user_data);
    char buf[64]{};
    int nr;
    struct sockaddr_in from{};
    socklen_t len{sizeof(from)};
    nr=recvfrom(fd,buf,sizeof(buf),0,(struct sockaddr*)&from,&len);
    LOG(TAG "receive '%.*s' from " SOCKADDR_FMT,nr,buf,SOCKADDR_OF(from));
    auto ip=inet_ntoa(from.sin_addr);
    auto input=Codec::Input{};
    input.parseIp(ip).parseWeight(buf).parseBlock(buf).parseSerial(buf);
//    collector->addPayload(std::string(buf,nr));
    auto x=Codec::Encode(input);
    collector->addPayload(Secure::Encrypt(&x,sizeof(x)));
    if(collector->db()){
        LOG("(DATABASE)save (IP)%s & (DATA)%s",ip,buf);
        collector->db()->set(1,ip)->set(2,buf)->execute();
    }
}

void Collector::HandlerInterrupt(int fd, void *user_data) {
    auto collector=reinterpret_cast<Collector*>(user_data);
    auto ss=Signal::Read(fd);
    LOG(TAG "receive signal #%d",ss.ssi_signo);
    collector->quit();
}

bool Collector::Connect() {
    if(fd_==-1){ /*to reuse socket resource*/
        int NODELAY=1;
        ERROR_RETURN((fd_=socket(AF_INET,SOCK_STREAM,0))==-1,false,,1);
        struct timeval tv{CONNECT_TIMEOUT,0};
        SETSOCKOPT(fd_,SOL_SOCKET,SO_SNDTIMEO,tv,false);
        SETSOCKOPT(fd_,IPPROTO_IP,TCP_NODELAY,NODELAY,false); /*disable the Nagle algorithm*/
    }
//    Cleaner/*go defer alike*/ cleaner{[this]{close(fd_);}};
    ERROR_RETURN(connect(fd_,SOCKADDR_EX(server_address_))==-1,false,,1);
//    cleaner.cancel();
    LOG(TAG "#%d connect to " SOCKADDR_FMT,fd_,SOCKADDR_OF(server_address_));
    return true;
}

void Collector::OnConnect() {
    CALL_START();
    if(state_.load(std::memory_order_acquire)==State::DISCONNECTED){
        LOG(TAG "disarm the timer");
        const struct itimerspec its{};
        timerfd_settime(timer_fd_,0,&its/*disarm timer*/,nullptr);
        Connection c{timer_fd_};
        c.epollUnregister(epoll_handler_);
        c.cancel();
//        for(auto&i:handlers_){
//            if(i->fd()==timer_fd_){
//                i->cancel(); /*avoid closing timer*/
//                handlers_.erase(i);
//                break;
//            }
//        }
        std::shared_ptr<Connection> handler;
        for(auto&i:handlers_){
            if(i->fd()==timer_fd_){
                handler=i;
                break;
            }
        }
        if(handler){
            handlers_.erase(handler);
            handler->cancel();
        }
        LOG(TAG "erase the timer(#%d) from handler list",timer_fd_);
    }
    auto handler=std::make_shared<Connection>(fd_);
    handler->epollRegister(epoll_handler_,EPOLLRDHUP);
    handler->registerCallback({nullptr,nullptr,[handler,this](int&){
        LOG(TAG "remote server is disconnected");
//        handler->cancel(); /*avoid closing client(this endpoint is marked as connected)*/
        handler->cancel();
        handlers_.erase(handler); /*release resource*/
//        fd_=-1; /*reset fd*/
        FdHelper::Close(fd_);
        OnDisconnect();
    }},nullptr);
    handlers_.insert(handler);
    retry_count=0;
    state_.store(State::READY,std::memory_order_release);
    CALL_END();
}

void Collector::OnDisconnect() {
    CALL_START();
    const struct itimerspec its{{RECONNECT_DURATION*2,0},{RECONNECT_DURATION,0}};
    LOG(TAG "arm the timer");
    timerfd_settime(timer_fd_,0,&its,nullptr);
    auto handler=std::make_shared<Connection>(timer_fd_);
    handler->epollRegister(epoll_handler_,EPOLLIN);
    handler->registerCallback({[this](int fd,void*user_data){
        unsigned long expiration{};
//        auto collector=reinterpret_cast<Collector*>(user_data);
        read(fd,&expiration,sizeof(expiration));
        LOG(TAG "#%ld try to connect server",(retry_count+=expiration));
        if(Connect()){
            OnConnect();
        }
    },nullptr,nullptr},nullptr);
    handlers_.insert(handler);
    state_.store(State::DISCONNECTED,std::memory_order_release);
    CALL_END();
}

#undef TAG