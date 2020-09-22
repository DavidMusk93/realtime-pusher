//
// Created by Steve on 9/1/2020.
//

#ifndef C4FUN_LOGGER_H
#define C4FUN_LOGGER_H

#include <thread>
#include <atomic>
#include <string>
#include <vector>

#include "tool.h"

class Logger final:public noncopyable{
public:
    enum class State:char{
        UNINITIALIZED,
        READY,
        RUNNING,
        DETACHED,
    };

private:
//    template<size_t N,size_t MB> /*compiler can not recognize this CTOR*/
    Logger(const std::string&logfile,size_t N,size_t MB,std::string buffer):handler_{nullptr},fds_{-1,-1},i_{0},buffer_(std::move(buffer)){
        filesize_=MB*kMb;
        logfiles_.resize(N);
        logbase_=logfile;
        for(auto&f:logfiles_){
            f.append(logfile+std::to_string(&f-&logfiles_[0]));
        }
        state_=State::UNINITIALIZED;
    }

public:
    Logger(Logger&&other);
    Logger&redirect();
    void detach();

    template<size_t N,size_t MB,size_t BL=1024/*buffer length*/>
    static Logger CreateLogger(const std::string&logfile){
        static_assert(N&&MB&&BL&&N<=10&&MB<=20&&BL<=1024);
        return Logger{logfile,N,MB,std::string(BL,0)};
    }

protected:
    void Run();
    void Next();

private:
    FILE*handler_;
    int fds_[2];
    size_t i_;
    size_t filesize_;
//    std::thread actor_;
    std::atomic<State> state_;
    std::vector<std::string> logfiles_;
    std::string logbase_;
    std::string buffer_;

    static constexpr const size_t kMb=1024*1024;
};

#endif //C4FUN_LOGGER_H
