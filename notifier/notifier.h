//
// Created by Steve on 9/1/2020.
//

#ifndef C4FUN_NOTIFIER_H
#define C4FUN_NOTIFIER_H

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>

#include "reader.h"

#define THREAD_KERNEL_TAG(tag,state) LOG("@THREAD_KERNEL ("#tag ") " #state)
#define THREAD_KERNEL_START(tag) THREAD_KERNEL_TAG(tag,START)
#define THREAD_KERNEL_END(tag) THREAD_KERNEL_TAG(tag,END)

class TopicManager{
public:
    using consumer_t=std::function<bool(const void*,size_t)>;
    using consumer_ptr=std::shared_ptr<consumer_t>;

    void subscribe(const std::vector<std::string>&topics,const consumer_ptr&ptr){
        for(auto&topic:topics){
            table_[topic].push_back(ptr);
        }
    }

    void publish(const std::string&topic,void*payload,size_t len){ //not thread safe
        std::vector<consumer_ptr> t;
        for(auto&fn:table_[topic]){
            if((*fn)(payload,len)){
                t.push_back(std::move(fn));
            }
        }
        table_[topic]=t;
    }

    std::vector<consumer_ptr>&operator[](const std::string&topic){
        return table_[topic];
    }

private:
    std::unordered_map<std::string,std::vector<consumer_ptr>> table_;
};


static inline FUNCTION_UNUSED void j2s(std::string&json){ //temporary
    auto k=json.find('w'); //locate weight
    if(k==std::string::npos){
        return;
    }
    auto i=json.find(':',k);
    auto j=json.find(',',k);
    if(i==std::string::npos||j==std::string::npos){
        return;
    }
    ++i;
    json=json.substr(i,j-i);
}

class Notifier{
public:
    using T=TopicManager::consumer_t;

    Notifier();
    ~Notifier();
    void Register(intptr_t key,T&& notify,const std::vector<std::string>&topics);

protected:
    void Run();

private:
//    RandomReader rand_{"/dev/urandom"};
    std::unique_ptr<Reader> reader_;
    std::unordered_map<intptr_t,std::weak_ptr<T>> subscriber_;
//    std::unordered_map<intptr_t,T> callback_;
    TopicManager manager_;
    std::thread actor_;
    std::atomic_flag flag_=ATOMIC_FLAG_INIT;
};

extern std::vector<std::string> parseTopic(const std::string&uri);
#endif //C4FUN_NOTIFIER_H
