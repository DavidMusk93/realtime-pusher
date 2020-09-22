//
// Created by Steve on 9/22/2020.
//

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "notifier.h"
#include "logger.h"

typedef websocketpp::server<websocketpp::config::asio> server;
typedef server::message_ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

static Notifier notifier;

static void on_message(server*s,websocketpp::connection_hdl hdl,message_ptr msg){
    auto uri=s->get_con_from_hdl(hdl)->get_uri()->str();
    LOG("%s called with hdl:%p & message:%s & uri:%s",__func__,hdl.lock().get(),msg->get_payload().c_str(),uri.c_str());
    auto topics=parseTopic(uri);
    if(!topics.empty()){
        auto op=msg->get_opcode();
        auto task=[s,hdl,op](const void*data,size_t len)->bool{
            try{
                s->send(hdl,data,len,op);
            }catch(const websocketpp::exception&){
                return false; /*send failure*/
            }
            return true; /*send success*/
        };
        notifier.Register(reinterpret_cast<intptr_t>(hdl.lock().get()),std::move(task),topics);
    }
}

int main(){
    auto logger=Logger::CreateLogger<10,10>("/tmp/notifier.log");
    if(ENABLE_LOGGER){logger.redirect().detach();}
    server server;
    try{
        server.set_access_channels(websocketpp::log::alevel::all);
        server.clear_access_channels(websocketpp::log::alevel::frame_payload);
        server.init_asio();
        server.set_message_handler(bind(&on_message,&server,::_1,::_2));
        server.listen(9002);
        server.start_accept();
        server.run(); /*starts the ASIO io_service run loop*/
    }catch(const websocketpp::exception&e){
        LOG("%s",e.what());
    }catch(...){
        LOG("other exception");
    }
}