//
// Created by Steve on 8/28/2020.
//

#ifndef C4FUN_CONNECTOR_BASE_H
#define C4FUN_CONNECTOR_BASE_H

#include <string>
#include <atomic>

class Connector{
public:
    enum class State:char{
        UNINITIALIZED,
        READY,
        PREPARING,
        EXECUTED,
    };

    struct Option{
        std::string uri,user,password;
    };
    virtual ~Connector()=default;
    virtual Connector*use(const std::string&db)=0;
    virtual intptr_t registerStatement(const std::string&statement)=0;

    virtual Connector*set(size_t index,const std::string&)=0;
//    template<size_t Index,typename T> virtual Connector*set(const T&t)=0
    virtual bool execute()=0;
    virtual bool ready()=0;

protected:
    std::atomic<State> state_{State::UNINITIALIZED};
};

#endif //C4FUN_CONNECTOR_BASE_H
