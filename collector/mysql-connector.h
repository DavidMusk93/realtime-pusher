//
// Created by Steve on 8/28/2020.
//

#ifndef C4FUN_MYSQL_CONNECTOR_H
#define C4FUN_MYSQL_CONNECTOR_H

#include <string>
#include <type_traits>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>

#include "connector-base.h"

class MysqlConnector:public Connector{
public:
    MysqlConnector(const char*uri,const char*user,const char*pwd);
    ~MysqlConnector() override;
    MysqlConnector*use(const std::string&db) override;

    intptr_t registerStatement(const std::string&statement) override;
    bool execute() override;


//    template<size_t Index, typename T> Connector*MysqlConnector::set(const T &t) override {
//        if constexpr (std::is_same_v<T,std::string>){
//            pstmt_->setString(Index,t);
//        }else if constexpr (std::is_same_v<T,int>){
//            pstmt_->setInt(Index,t);
//        }
//        return this;
//    }
    MysqlConnector*set(size_t index, const std::string &string) override;

    bool ready() override;

private:
    bool Connect(const Connector::Option&option);

private:
    Connector::Option option_;
    sql::Driver*driver_;
    sql::Connection*con_;
    sql::PreparedStatement*pstmt_;

};

#endif //C4FUN_MYSQL_CONNECTOR_H
