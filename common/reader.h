//
// Created by Steve on 8/26/2020.
//

#ifndef C4FUN_READER_H
#define C4FUN_READER_H

#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

#include "tool.h"

class Reader{
public:
    using u8=unsigned char;
    using u16=unsigned short;
    using u32=unsigned int;
    using u64=unsigned long;

    using Filter=std::function<bool(const std::string&)>;

public:
    Reader():fd_(-1){}
    virtual ~Reader()=default;

    virtual bool Eof()=0;
    virtual short GetShort()=0;
    virtual int GetInt()=0;
    virtual int GetObserveFd()=0;
    virtual void registerFilter(Filter&&filter){}
protected:
    int&GetFd(){
        return fd_;
    }

private:
    int fd_;
};

#define ERROR_RETURN(error,code,block,fmt,...) \
do{\
    if(error){\
        LOG(fmt,##__VA_ARGS__);\
        block\
        return code;\
    }\
}while(0)

class RandomReader:public Reader{
public:
    RandomReader(const std::string&file):Reader(),i_(kBufferSize){
        int fd=-1;
        ERROR_RETURN((fd=open(file.c_str(),O_RDONLY))==-1,,,"fail to open %s: %s",file.c_str(),strerror(errno));
        GetFd()=fd;
    }

    ~RandomReader() override{
        close(GetFd());
    }

    bool Eof() override {
        return false;
    }

    short GetShort() override {
        short x{};
        Fetch(&x,sizeof x);
        return x;
    }

    int GetInt() override {
        return 0;
    }

    int GetObserveFd() override {
        return 0;
    }

    template<typename T>
    T fetch(){
        T/*trivial type*/ t{};
        Fetch(&t,sizeof(t));
        return t;
    }

protected:
    void Fetch(void *data,size_t len){
        if(len){
            auto p=reinterpret_cast<char*>(data);
            while(len--){
                if(i_+1>kBufferSize){
                    i_=0;
                    read(GetFd(),buf_,kBufferSize);
                }
                *p++=buf_[i_++];
            }
        }
    }
private:
    int i_;
    char buf_[1024];

    static constexpr int kBufferSize=sizeof(buf_);
};

#undef ERROR_RETURN

#endif //C4FUN_READER_H
