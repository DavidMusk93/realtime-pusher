//
// Created by Steve on 8/11/2020.
//

#include "notifier.h"
#include "codec.h"
#include "reader.h"
#include "tool.h"
#include "secure.h"
#include "network.h"

Terminator terminator;

class RemoteReader:public Reader{
#define TAG "(RELAY)"
public:
    static constexpr short PORT=10001;
    static constexpr int BACKLOG=5;
    static constexpr int BAD_VALUE=-0xffff;

public:
    RemoteReader():Reader(),fds_{-1,-1},state_(State::WAIT),x_(BAD_VALUE){
        int sock;
        pipe2(fds_,O_NONBLOCK);
        ERROR_RETURN((sock=socket(AF_INET,SOCK_STREAM,0))==-1,,,1);
        Cleaner cleaner{[&sock](){close(sock);}};
        struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(PORT),.sin_addr={INADDR_ANY},};
        int REUSE=1;
        int QUICKACK=1;
        SETSOCKOPT(sock,SOL_SOCKET,SO_REUSEADDR,REUSE,);
        SETSOCKOPT(sock,SOL_SOCKET,SO_REUSEPORT,REUSE,);
        SETSOCKOPT(sock,IPPROTO_IP,TCP_QUICKACK,QUICKACK,); /*acks are sent immediately*/
        ERROR_RETURN(bind(sock,SOCKADDR_EX(addr))==-1,,,1);
        ERROR_RETURN(listen(sock,BACKLOG)==-1,,,1);
        cleaner.cancel();
        GetFd()=sock;
        actor_=std::thread(&RemoteReader::Run,this);
    }

    ~RemoteReader() override{
        terminator.trigger();
        if(actor_.joinable()){
            actor_.join();
        }
        FdHelper::Close(GetFd());
        FdHelper::Close(fds_[0]);
        FdHelper::Close(fds_[1]);
    }

    bool Eof() override {
        return false;
    }

    short GetShort() override {
        return 0;
    }

    int GetInt() override {
        if(std::atomic_load_explicit(&state_,std::memory_order_relaxed)==State::READY){
            return std::atomic_load_explicit(&x_,std::memory_order_acquire);
        }
        return BAD_VALUE;
    }

    int GetObserveFd() override {
        return fds_[FdHelper::kPipeRead];
    }

protected:
    enum class State{
        READY,
        WAIT,
        QUIT,
    };

    void Run(){
        THREAD_KERNEL_START(RELAY);
#define MAX_EVENTS 10
#define TIMEOUT 5000/*ms*/
        struct epoll_event events[MAX_EVENTS]{};
        int nfds,epollfd;
//        std::unordered_set<Connection::Handler> handlers;
        std::unordered_map<int,Connection::Handler> handlers;
        Connection::Handler handler;
        ERROR_RETURN((epollfd=epoll_create1(0))==-1,,,1);
        auto handle_message=[this,epollfd,&handlers](int fd,void*)->void{
            LOG(TAG "tcp unread size:%d",FdHelper::UnreadSize(fd));
            Recorder r1{"RECEIVE SINGLE MESSAGE"};
            auto handler=handlers[fd];
            auto&buf=handler->Buffer();
//            unsigned long x{};
            Codec::Data x{};
            recv(fd,&buf[0],buf.size(),MSG_WAITALL); /*block read*/
            if(!Secure::Decrypt(buf,&x,sizeof(x))){
                LOG(TAG "illegal peer,kick out #%d",fd);
                handlers.erase(fd);
                return;
            }
            auto topic=Codec::Output::RetrieveTopic(x.b);
            LOG(TAG "new message:topic,%s;serial:%u",topic.c_str(),x.a);
            if(filter_&&!filter_(topic)){
                LOG(TAG "no subscriber,drop payload");
                return;
            }
            int ub=FdHelper::UnreadSize(GetObserveFd());
            if(ub<128){ /*avoid filling up the pipe*/
                auto nw=write(fds_[FdHelper::kPipeWrite],&x,sizeof(x));
                if(nw==-1){
                    LOG(TAG "write failed:%s",ERROR_S);
                }
            }else{
                LOG(TAG "buffer is full,drop payload");
            }
            std::atomic_store_explicit(&state_,State::READY,std::memory_order_release);
        };
        auto handle_connection=[this,epollfd,handle_message,&handlers](int fd,void*)->void{
            int sock{-1};
            struct sockaddr_in peer{};
            socklen_t len{sizeof(peer)};
            ERROR_RETURN((sock=accept4(fd,(struct sockaddr*)&peer,&len,O_NONBLOCK))==-1,,,1);
            LOG(TAG "new connection from %s:%d",inet_ntoa(peer.sin_addr),ntohs(peer.sin_port));
            /*register active fd*/
            auto handler=std::make_shared<Connection>(sock);
            if(handler->epollRegister(epollfd,EPOLLIN|/*EPOLLET|EPOLLHUP|*/EPOLLRDHUP)==0/*error prone*/){
                handler->Buffer()=Secure::CreateBuffer();
                handler->registerOnRecv(handle_message)->registerOnClose([&handlers](int&fd){
                    LOG(TAG "#%d remote close",fd);
                    handlers.erase(fd);
                });
                handlers.insert({handler->fd(),handler});
            }
        };
        Cleaner cleaner{[epollfd](){close(epollfd);}};

        /*register passive fd*/
        handler=std::make_shared<Connection>(GetFd());
        handler->epollRegister(epollfd,EPOLLIN);
        handler->registerOnRecv(handle_connection);
        handlers.insert({handler->fd(),handler});

        /*register terminator fd*/
        handler=std::make_shared<Connection>(terminator.observeFd());
        handler->registerOnRecv([this](int,void*){state_.store(State::QUIT,std::memory_order_relaxed);});
        handlers.insert({handler->fd(),handler});
        while(state_.load(std::memory_order_relaxed)!=State::QUIT){
            POLL(nfds,epoll_wait,epollfd,events,MAX_EVENTS,/*TIMEOUT*/-1);
//            if(nfds==0){
//                std::atomic_store_explicit(&state_,State::WAIT,std::memory_order_relaxed);
//            }
            for(int i=0;i<nfds;++i){
                handlers[events[i].data.fd]->eventTrigger(epollfd,events[i].events);
            }
        }
#undef TIMEOUT
#undef MAX_EVENTS
        THREAD_KERNEL_END(RELAY);
    }

public:
    void registerFilter(Filter &&filter) override {
        filter_.swap(filter);
    }

private:
    int fds_[2];
    std::atomic<State> state_;
    std::atomic<int> x_;
    std::thread actor_;
    Filter filter_;
#undef SOCKADDR_EX
#undef TAG
};

Notifier::Notifier(){
//        reader_=std::make_unique<RandomReader>("/dev/urandom");
//        reader_=std::make_unique<RemoteReader>(); //c++14 required
    reader_.reset(new RemoteReader());
    reader_->registerFilter([this](const std::string&topic)->bool{return !manager_[topic].empty();});
    actor_=std::thread(&Notifier::Run,this); //danger if it is initialized in constructor member list
}

Notifier::~Notifier(){
    if(actor_.joinable()){
        terminator.trigger();
        actor_.join();
    }
}

void Notifier::Register(intptr_t key, T &&notify, const std::vector<std::string> &topics){
    Spinlock lock{flag_};
    if(subscriber_.count(key)){
        return;
    }
    auto task=std::make_shared<T>(std::move(notify));
    manager_.subscribe(topics,task);
    subscriber_.insert({key,std::weak_ptr<T>(task)});
    LOG("(NOTIFIER)subscriber count #%ld",subscriber_.size());
}

void Notifier::Run(){
    THREAD_KERNEL_START(NOTIFIER);
    struct pollfd pfd[2]={
            {.fd=terminator.observeFd(),.events=POLLIN},
            {.fd=reader_->GetObserveFd(),.events=POLLIN},
    };
    int nfds{};
    for(;;){
        POLL(nfds,poll,pfd,2,-1);
//            ERROR_RETURN(nfds==-1,,{exit(1);/*force quit*/},"poll: %s",ERROR_S);
//            if(nfds==0){
//                continue;
//            }
        if(pfd[0].revents&POLLIN){
            break;
        }
        Recorder r1{"CONSUME SINGLE MESSAGE"};
        auto&fd=pfd[1].fd;
//        unsigned long x{};
        Codec::Data x{};
        read(fd,&x,sizeof(x));
        auto output=Codec::Decode(x);
        std::vector<std::shared_ptr<T>> tasks;
        {
            Spinlock lock(flag_);
            std::vector<intptr_t> invalid;
            for(auto&p:subscriber_){
                if(!p.second.lock()){ /*try promotion*/
//                    subscriber_.erase(p.first);
                    invalid.push_back(p.first);
                }
            }
            for(auto&k:invalid){
                LOG("erase handler %p",reinterpret_cast<void*>(k));
                subscriber_.erase(k);
            }
            auto&ref=manager_[output.topic];
            if(ref.empty()){
//                LOG("(NOTIFIER)no subscriber,drop '%s'",output.payload.c_str());
                continue;
            }
            tasks.swap(ref);
        }
        std::vector<std::shared_ptr<T>> valid;
//            j2s(output.payload);
        LOG("(NOTIFIER)@TOPIC %s subscriber count:%ld",output.topic.c_str(),tasks.size());
        for(auto&p:tasks){
            Recorder r2{"WEBSOCKET PUBLISH"};
            if((*p)(output.payload.data(),output.payload.size())){ /*real publish*/
                valid.push_back(std::move(p));
            }else{
                r2.Cancel();
            }
        }
        if(valid.empty()){
            continue;
        }
        Spinlock lock(flag_);
        auto&ref=manager_[output.topic];
        for(auto&k:valid){
            ref.push_back(std::move(k));
        }
    }
    THREAD_KERNEL_END(NOTIFIER);
}

std::vector<std::string> parseTopic(const std::string&uri){
    std::vector<std::string> res;
    char k[16],v[16];
    auto i=uri.find('?');
    if(i==std::string::npos){
        return {};
    }
    while(++i/*jump &*/<uri.size()){
        int n{};
        sscanf(&uri[i],"%[^=]=%[^&]%n",k,v,&n);
        LOG("@TOPIC %s:%s",k,v);
        res.emplace_back(v);
        i+=n;
    }
    return res;
}

//int main(){
//    std::string uri="ws://192.168.10.101:9001?w1=182&w2=184";
//    for(auto&i:parseTopic(uri)){
//        LOG("%s",i.c_str());
//    }
//    std::string json=R"({"topic":"%s","weight":256,"id":"%s"})";
//    j2s(json);
//    LOG("%s",json.c_str());
//}