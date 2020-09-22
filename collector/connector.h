//
// Created by Steve on 8/28/2020.
//

#ifndef C4FUN_CONNECTOR_H
#define C4FUN_CONNECTOR_H

#include <string>
#include <memory>
#include <atomic>

#include "mysql-connector.h"

#define MYSQL_URI "tcp://127.0.0.1:3306"
#define MYSQL_USER "test"
#define MYSQL_PWD "1234567890"

#define DB_NAME "datacenter"
#define TABLE_NAME "record"
#define SQL_STATEMENT "INSERT INTO " TABLE_NAME "(ip, data) VALUES (?,?)"


class ConnectorFactory{
public:
    enum DatabaseType{
        MYSQL,
    };

    static Connector*create(int type){
        if(type==MYSQL){
            return new MysqlConnector(MYSQL_URI,MYSQL_USER,MYSQL_PWD);
        }
        return nullptr;
    }
};

#endif //C4FUN_CONNECTOR_H
