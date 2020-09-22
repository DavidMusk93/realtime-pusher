//
// Created by Steve on 8/28/2020.
//

#include "mysql-connector.h"
#include "tool.h"

MysqlConnector::MysqlConnector(const char *uri, const char *user, const char *pwd):option_({uri,user,pwd}),con_(nullptr),pstmt_(nullptr) {
    driver_=get_driver_instance();
    Connect(option_);
}

MysqlConnector::~MysqlConnector() {
    delete con_;
    delete pstmt_;
}

bool MysqlConnector::Connect(const Connector::Option &option) {
    try{
        con_=driver_->connect(option.uri,option.user,option.password);
    } catch (sql::SQLException&e) {
        LOG("(MYSQL_CONNECTOR)connect failed: %s",e.what());
        return false;
    }
    state_.store(Connector::State::READY);
    return true;
}

MysqlConnector*MysqlConnector::use(const std::string &db) {
    con_->setSchema(db);
    return this;
}

intptr_t MysqlConnector::registerStatement(const std::string &statement) {
    pstmt_=con_->prepareStatement(statement);
    return reinterpret_cast<intptr_t >(pstmt_);
}

bool MysqlConnector::execute() {
    try {
        pstmt_->executeUpdate();
    } catch (sql::SQLException&e) {
        LOG("(MYSQL_CONNECTOR)execute failed: %s",e.what());
        return false;
    }
    return true;
}

bool MysqlConnector::ready() {
    return state_!=Connector::State::UNINITIALIZED;
}

MysqlConnector*MysqlConnector::set(size_t index, const std::string &string) {
    pstmt_->setString(index,string);
    return this;
}

//#include "connector.h"
//
//int main(){
//    std::unique_ptr<Connector> db;
//    db.reset(ConnectorFactory::create(ConnectorFactory::MYSQL));
//    if(db->ready()){
//        db->use(DB_NAME)->registerStatement(SQL_STATEMENT);
//        db->set(1,"192.168.10.101")->set(2,"1@1@1")->execute();
//    }
//}