//
// Created by Steve on 9/1/2020.
//

#include "logger.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>

Logger::Logger(Logger &&other) {
    handler_=other.handler_;
    memcpy(fds_,other.fds_,2*sizeof(int));
    i_=other.i_;
    filesize_=other.i_;
    state_.store(other.state_);
    logfiles_.swap(other.logfiles_);
    logbase_.swap(other.logbase_);
    buffer_.swap(other.buffer_);
}

Logger&Logger::redirect() {
    if(state_==State::UNINITIALIZED){
        pipe2(fds_,O_NONBLOCK);
        dup2(fds_[FdHelper::kPipeWrite],STDOUT_FILENO);
        dup2(fds_[FdHelper::kPipeWrite],STDERR_FILENO);
        setbuf(stdout,nullptr);
        setbuf(stderr,nullptr);
        state_=State::READY;
    }
    return *this;
}

void Logger::detach() {
    if(state_==State::DETACHED){
        return;
    }
    auto actor=std::thread(&Logger::Run,this);
    while(state_.load(std::memory_order_acquire)!=State::RUNNING);
    actor.detach();
    state_=State::DETACHED;
}

void Logger::Run() {
#define FREQUENCY 100
    struct pollfd pfd{.fd=fds_[FdHelper::kPipeRead],.events=POLLIN};
    state_.store(State::RUNNING,std::memory_order_release);
    int nfds{},nr{},count{};
    char*buf=&buffer_[0];
    const int BL=buffer_.size();
    Next();
    LOG("(LOGGER)%s,%zu,%#zx",logbase_.c_str(),logfiles_.size(),filesize_);
    for(;;){
        POLL(nfds,poll,&pfd,1,-1);
        nr=read(pfd.fd,buf,BL);
        fprintf(handler_,"%.*s",nr,buf);
        fflush(handler_); /*key to inspect realtime log*/
        if(++count>=FREQUENCY){
            count=0;
            size_t n=ftell(handler_);
            if(n>filesize_){
                Next();
            }
        }
    }
#undef FREQUENCY
}

void Logger::Next() {
    if(!handler_){
        const char*from=logfiles_[i_].c_str(),*to=logbase_.c_str();
        unlink(to);
        symlink(from,to);
        handler_=fopen(to,"w");
    }else{
        if(++i_==logfiles_.size()){
            i_=0;
        }
        fclose(handler_);
        handler_=nullptr;
        Next();
    }
}