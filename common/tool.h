//
// Created by Steve on 8/25/2020.
//

#ifndef C4FUN_TOOL_H
#define C4FUN_TOOL_H

#include <unistd.h>
#include <sys/signal.h>
#include <sys/signalfd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/socket.h>

#include <atomic>
#include <functional>

#define ENABLE_DB     getenv("ENABLE_DB")
#define ENABLE_LOGGER getenv("ENABLE_LOGGER")

#define ERROR_S strerror(errno)

#define NOW() ({\
    timeval __tv{};\
    gettimeofday(&__tv,nullptr);/*VDSO(Virtual Dynamic Shared Object)*/\
    __tv;\
})
#define SECONDS sun::time::seconds(NOW())

#define TIMETHIS(expr) \
do{\
    auto t1=SECONDS;\
    expr;\
    auto t2=SECONDS;\
    printf(#expr "cost #%f (s)\n",t2-t1);\
}while(0)

#define USE_OS_TID 1
#if USE_OS_TID
#define TID_FMT "%lu"
#else
#define TID_FMT "%#lx"
#endif

#define LOG(fmt,...) printf("%s " TID_FMT " " fmt "\n",sun::time::format(NOW()),sun::utility::tid(),##__VA_ARGS__)

#define __TRIVIAL()
#define POLL(__pr,__fn,...) \
__pr=__fn(__VA_ARGS__);\
if(__pr<0){\
    if(errno==EINTR||errno==EAGAIN){continue;}\
    LOG(#__fn ":%s",ERROR_S);\
    break;\
}
__TRIVIAL()

#define __ATTRIBUTE(keyword) __attribute__((keyword))
#define FUNCTION_UNUSED __ATTRIBUTE(unused)
#define TYPE_PACKED __ATTRIBUTE(packed)

namespace sun/*nested namespace definition is a C++1z extension*/ {
    namespace time {
        extern timeval now();

        extern double seconds(const timeval &tv);

        extern const char *format(const timeval &tv);
    }
    namespace utility{
        unsigned long tid();
        unsigned long pid();
    }
}

class Signal{
public:
    Signal(){sigemptyset(&mask_);}
    Signal&registerSignal(int signum);
    int fd();

public:
    static signalfd_siginfo Read(int fd){
        signalfd_siginfo ss{};
        read(fd,&ss,sizeof(ss));
        return ss;
    }

private:
    sigset_t mask_{};
    int fd_{-1};
};

class Counter{
public:
    Counter();
    void tick();

private:
    void Reset(double ts){
        count_=0;
        ts_=ts;
    }

private:
    size_t count_;
    double ts_;

    static constexpr const size_t MAX=30;
};

class noncopyable{
public:
    noncopyable(const noncopyable&)=delete;
    noncopyable&operator=(const noncopyable&)=delete;

protected:
    noncopyable()=default;
    ~noncopyable()=default;
};

class Spinlock{
public:
    Spinlock(std::atomic_flag&flag):flag_(flag){
        while(flag_.test_and_set(std::memory_order_acquire));
    }
    ~Spinlock(){
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag&flag_;
};

class FdHelper{
public:
    FdHelper()=delete;

    static void SetNonblock(int fd){
        int flags;
        fcntl(fd,F_GETFL,&flags);
        fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    }

    static int UnreadSize(int fd){
        int size{};
        ioctl(fd,FIONREAD,&size);
        return size;
    }

//    static void Drain(int sock){
//        for(;;){
//            ssize_t nr=recv(sock,nullptr,0,MSG_DONTWAIT); /*undefined action*/
//            if(nr<=0){break;}
//        }
//    }

    static void Close(int&fd){
        if(fd!=-1){
            close(fd);
            fd=-1;
        }
    }
    static void Close(int&fd1,int&fd2){
        Close(fd1),Close(fd2);
    }
    static void Close(int&fd1,int&fd2,int&fd3){
        Close(fd1),Close(fd2,fd3);
    }

public:
    static constexpr int kPipeRead=0;
    static constexpr int kPipeWrite=1;
};

class Terminator{
public:
    Terminator():fds_{-1,-1}{
        pipe2(fds_,O_NONBLOCK);
    }

    ~Terminator(){
        close(fds_[0]),close(fds_[1]);
        fds_[0]=fds_[1]=-1;
    }

    int observeFd(){
        return fds_[FdHelper::kPipeRead];
    }

    void trigger(){
        char c='Q';
        write(fds_[FdHelper::kPipeWrite],&c,1);
    }

    void cleanup(){
        char c;
        read(fds_[FdHelper::kPipeRead],&c,1);
    }

private:
    int fds_[2];
};

using Closure=std::function<void()>;

class Cleaner{
public:
    Cleaner(Closure closure):final_(std::move(closure)){}
    Cleaner&operator=(Cleaner&&cleaner)noexcept{
        final_.swap(cleaner.final_);
        return *this;
    }
    ~Cleaner(){
        if(final_){
            final_();
        }
    }

    void cancel(){
        Closure trivial;
        final_.swap(trivial);
    }

private:
    Closure final_;
};

class Recorder{
public:
    Recorder(std::string tag):tag_(std::move(tag)),emit_(false){
        ts_=SECONDS;
    }
    Recorder(Recorder&&other){
//        tag_.swap(other.tag_);
//        ts_=other.ts_;
        *this=std::move(other);
    }
    Recorder&operator=(Recorder&&other) noexcept{
        tag_.swap(other.tag_);
        ts_=other.ts_;
        emit_=other.emit_;
        return *this;
    }
    ~Recorder(){
        Done();
    }
    void Done(){
        if(!emit_){
            LOG("@RECORDER<%s> cost %f(s)",tag_.c_str(),SECONDS-ts_);
            emit_=true;
        }
    }
    void Cancel(){
        emit_=true;
    }
private:
    std::string tag_;
    double ts_;
    bool emit_;
};

#endif //C4FUN_TOOL_H
