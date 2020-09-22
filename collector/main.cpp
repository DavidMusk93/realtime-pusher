//
// Created by Steve on 9/22/2020.
//

#include "collector.h"
#include "logger.h"

template<typename T>
std::vector<T> split(const char*s,char separator,const std::function<T(const std::string&)>& op){
    std::vector<T> res;
    size_t i,j;
    for(i=0,j=0;s[j];++j){
        if(s[j]==separator){
            if(j>i){
                res.push_back(op(std::string{&s[i],j-i}));
            }
            i=j+1;
        }
    }
    if(j>i){
        res.push_back(op(std::string{&s[i],j-i}));
    }
    return res;
}

int main(int argc,char*argv[]){
#define PORT_BASE 10000
#define GROUP_IP "228.67.43.91"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 10001
    if(argc<2){
        return 1;
    }
    auto logger=Logger::CreateLogger<10,1>("/tmp/collector.log");
    if(ENABLE_LOGGER){logger.redirect().detach();}
    auto ports=split<short>(argv[1],',',[](const std::string&s)->short{return (std::stoi(s)&0xffu)+PORT_BASE;});
    Collector collector{SERVER_IP,SERVER_PORT};
    collector.registerSource(Signal().registerSignal(SIGINT).registerSignal(SIGQUIT).fd(), &Collector::HandlerInterrupt);
    for(auto port:ports){
        collector.registerSource(Group(GROUP_IP,port).fd()/*transfer ownership*/, &Collector::HandleGroupMsg);
    }
    collector.loop();
}