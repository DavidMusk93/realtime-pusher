//
// Created by Steve on 8/27/2020.
//

#ifndef C4FUN_COLLECTOR_H
#define C4FUN_COLLECTOR_H

#include "network.h"
#include "connector.h"

#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <type_traits>

#include <sys/timerfd.h>

namespace sun{
    template<typename T>
    class Queue final:public noncopyable{
#define LOCK() std::unique_lock<std::mutex> lock(mutex_)
    public:
//        Queue()=default;
        ~Queue(){finish();}
        void offer(T&&t){
            LOCK();
            data_.push_back(/*std::forward<T>(t)*/std::move(t));
            cv_.notify_one();
        }
        bool poll(T&t){ /*slow consumer*/
            LOCK();
            cv_.wait(lock,[this](){return finish_||!data_.empty();});
            if(finish_){
                return false;
            }
            t.swap(data_.front());
            data_.pop_front();
            return true;
        }
        void finish(){
            LOCK();
            finish_=true;
            cv_.notify_all();
        }
    private:
        std::deque<T> data_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool finish_{false};
    };

    template<size_t N,size_t BLOCK>
    class Pool final:public noncopyable{
    public:
//        using header_t=size_t ;
        using block_t=void*;
        using release_t=std::function<void(block_t)>;
        using value_type=std::unique_ptr<void*,release_t>;

        Pool():/*size_(0),capacity_(N),*/block_(BLOCK){
            static_assert(N<=1024&&BLOCK<=1024*1024);
            const size_t MASK=sizeof(void*)-1;
//            if(block_&MASK){
//                block_=(block_&(~MASK))+sizeof(header_t)*2;
//            }else{
//                block_+=sizeof(header_t);
//            }
            if(block_&MASK){
                block_=(block_&(~MASK))+sizeof(void*);
            }
            data_=malloc(N*block_);
            stack_.resize(N);
            for(size_t i=0;i<N;++i){
                stack_[i]=reinterpret_cast<void*>((char*)data_+i*block_);
            }
        }
        ~Pool(){
            free(data_);
        }

        value_type acquire(){
            Spinlock sl{flag_};
            if(!stack_.empty()){
                void*v=stack_.back();
                stack_.pop_back();
                return value_type{new(v)block_t,[this](void*v){Spinlock sl{flag_};stack_.push_back(v);}};
            }else{
                void*v=malloc(block_);
                return value_type{new(v)block_t,[](void*v){free(v);}};
            }
        }

    private:
        void*data_;
//        size_t size_;
//        size_t capacity_;
        size_t block_;
        std::vector<void*> stack_;
        std::atomic_flag flag_=ATOMIC_FLAG_INIT;
    };
}

class Collector{
public:
#define TAG "(COLLECTOR)"
#define CONNECT_TIMEOUT 1/*s*/
#define RECONNECT_DURATION 5/*s*/
//    struct Payload{
//        char buf[64]{};
//        int len{};
//    };
#define PRODUCER_HANDLER event_handler_[0]
#define CONSUMER_HANDLER event_handler_[1]

    enum class State:char{
        UNINITIALIZED,
        READY,
        DISCONNECTED,
        DONE,
    };

    Collector(const std::string&server_ip,short server_port):fd_(-1),epoll_handler_(-1),event_handler_{-1,-1}{
        state_.store(State::UNINITIALIZED,std::memory_order_relaxed);
        MAKE_SOCKADDR_IN(server,inet_addr(server_ip.c_str()),htons(server_port));
        server_address_=server;
        if(Connect()){
            ERROR_RETURN((epoll_handler_=epoll_create1(0))==-1,,,1);
            ERROR_RETURN((timer_fd_=timerfd_create(CLOCK_REALTIME,O_NONBLOCK|O_CLOEXEC))==-1,,,1);
            if(ENABLE_DB/*dynamic loading*/){
                database_.reset(ConnectorFactory::create(ConnectorFactory::MYSQL));
            }
            if(database_&&database_->ready()){
                database_->use(DB_NAME)->registerStatement(SQL_STATEMENT);
            }else{
                database_.reset();
            }
            socketpair(AF_UNIX,SOCK_STREAM,0,event_handler_);
            state_.store(State::READY,std::memory_order_relaxed);
        }
    }
    ~Collector(){
        quit();
        FdHelper::Close(fd_,epoll_handler_,timer_fd_);
    }
    bool registerSource(int fd,decltype/*build-in?*/(std::declval<Connection::Callback>().on_recv) on_recv){
        if(epoll_handler_==-1){
//            LOG("(COLLECTOR)epoll instance has not created");
            close(fd);
            return false;
        }
        auto handler=std::make_shared<Connection>(fd);
        ERROR_RETURN(handler->epollRegister(epoll_handler_,EPOLLIN|/*EPOLLET|*/EPOLLRDHUP)==-1,false,,0);
        handler->registerCallback({std::move(on_recv),nullptr,[handler,this](int&){handlers_.erase(handler);}},this);
        handlers_.insert(handler);
        return true;
    }
    void addPayload(std::string payload){
#define READY_CHAR 'R'
//        payloads_.offer(std::move(payload));
        {
            Spinlock spinlock{flag};
            pv.push_back(std::move(payload));
        }
        char c=READY_CHAR;
        write(PRODUCER_HANDLER,&c,1);
    }

    void loop(){
        if(Ready()){
//            producer_=std::thread(&Collector::Produce,this);
            consumer_=std::thread(&Collector::Consume,this);
            Produce();
        }
    }

    void quit(){
#define JOIN(x) \
do{\
    if(x.joinable()){x.join();}\
}while(0)
//        payloads_.finish();
        terminator_.trigger();
//        JOIN(producer_);
        char quit[]="QUIT";
        write(PRODUCER_HANDLER,quit,sizeof(quit));
        JOIN(consumer_);
#undef JOIN
    }

public:
    static void HandleGroupMsg(int fd, void*user_data);
    static void HandlerInterrupt(int fd, void*user_data);

protected:
    void Produce(){
#define MAX_EVENTS 10
        int nfds;
        struct epoll_event events[MAX_EVENTS];
        Connection c{terminator_.observeFd()};
        ERROR_RETURN(c.epollRegister(epoll_handler_,EPOLLIN)==-1,,,0);
        c.cancel();
        OnConnect();
        for(;;){
//            ERROR_RETURN((nfds=epoll_wait(epoll_handler_,events,MAX_EVENTS,-1))==-1,,,1);
            POLL(nfds,epoll_wait,epoll_handler_,events,MAX_EVENTS,-1);
            for(int i=0;i<nfds;++i){
                const auto&fd=events[i].data.fd;
                if(fd==terminator_.observeFd()){
                    terminator_.cleanup();
                    state_.store(State::DONE,std::memory_order_release);
                    goto end;
                }
                auto handlers=handlers_;
                for(auto&handler:handlers/*callback may modify handlers_*/){
                    if(handler->fd()==fd){
                        handler->eventTrigger(epoll_handler_,events[i].events);
                        break; /*gotcha, then the next*/
                    }
                }
            }
        }
end:
        return;
#undef MAX_EVENTS
    }
    void Consume(){
#ifdef TIMETHIS
#undef TIMETHIS
#define TIMETHIS(expr) expr
#endif
        Counter counter;
        for(;;){
            if(state_.load(std::memory_order_acquire)==State::DONE){
                break;
            }
            char c{};
            read(CONSUMER_HANDLER,&c,1); /*block read*/
            if(c!=READY_CHAR){
                break;
            }
            {
                Spinlock spinlock{flag};
                cv.swap(pv); /*fast swap*/
                if(cv.empty()){continue;}
            }
            for(auto&payload:cv){
                counter.tick();
                if(state_.load(std::memory_order_acquire)==State::READY){
                    TIMETHIS(send(fd_,payload.data(),payload.size(),0/*MSG_MORE*/));
                }
            }
            cv.clear(); /*clear consumer container*/
        }
    }

private:
    bool Ready() const{
//        return fd_!=-1&&epoll_handler_!=-1&&!handlers_.empty();
        return state_.load(std::memory_order_relaxed)!=State::UNINITIALIZED;
    }

    std::unique_ptr<Connector>&db(){
        return database_;
    }

    void OnConnect();
    void OnDisconnect();
    bool Connect();

private:
    struct sockaddr_in server_address_;
    int fd_;
    int epoll_handler_;
    int timer_fd_;
    unsigned long retry_count{};
    std::atomic<State> state_;
//    sun::Queue<std::string> payloads_; /*slowly*/
    std::unordered_set<Connection::Handler> handlers_;
    Terminator terminator_;
    std::thread /*producer_,*/consumer_;
    std::unique_ptr<Connector> database_;
    std::vector<std::string> cv,pv;
    std::atomic_flag flag=ATOMIC_FLAG_INIT;
    int event_handler_[2];
};

#endif //C4FUN_COLLECTOR_H
