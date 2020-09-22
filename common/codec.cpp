//
// Created by Steve on 8/26/2020.
//

#include "codec.h"

#include <cstring>
#include <cstdlib>

Codec::Input & Codec::Input::parseIp(const char *data) {
#define DOT '.'
    auto dot=strrchr(data,DOT);
    if(dot){
        ip=atoi(dot+1)&IP_MASK;
    }
    return *this;
}

Codec::Input & Codec::Input::parseWeight(const char *data) {
#define SEP '@'
#define BAD "-65535"
#define BAD_LEN 6
    auto sep=strrchr(data,SEP);
    if(sep){
        if(strncmp(sep+1,BAD,BAD_LEN)==0){
            error=1;
            weight=WEIGHT_MASK;
        }else{
            weight=(atoi(sep+1)/*+WEIGHT_COMPLEMENT*/)&WEIGHT_MASK;
        }
    }
    return *this;
}

Codec::Input & Codec::Input::parseBlock(const char *data) {
    auto sep=strchr(data,SEP);
    if(sep){
        block=atoi(sep+1);
    }
    return *this;
}

Codec::Input &Codec::Input::parseSerial(const char *data) {
//    serial=atoi(data);
    char*end{};
    auto l=strtoull(data,&end,10);
    serial=static_cast<Codec::u32>(l);
    return *this;
}

static const char*i2s(Codec::u32 x){
#define LEN 4 /*sizeof(int)*/
    static __thread char buf[16];
    static const char*cs="0123456789ABCDEF";
    union{Codec::u32 x;Codec::u8 p[LEN];} u{x};
    char *p=&buf[LEN*2];
    *p--=0;
    for(auto i:u.p){
        *p--=cs[i&0xfu];
        *p--=cs[(i>>4u)&0xfu];
    }
    return buf;
#undef LEN
}

Codec::Output & Codec::Output::load(Codec::Data data) {
#define PATTERN "{\"serial\":%u,\"topic\":\"%s\",\"weight\":%d,\"id\":\"%s\"}"
    char buf[64];
    auto x=data.b;
    auto error=static_cast<u8>(x>>ERROR_OFFSET);
    auto block=x&BLOCK_MASK;
    topic=RetrieveTopic(x);
    sprintf(buf,PATTERN,data.a,
            topic.c_str(),
            error?atoi(BAD):static_cast<short>((x>>WEIGHT_OFFSET)&WEIGHT_MASK),
            block?i2s(x&BLOCK_MASK):"NONE");
    payload=buf;
    return *this;
}

Codec::Data Codec::Encode(const Input &input) {
    u64 x{};
    x|=static_cast<u64>(input.error)<<ERROR_OFFSET;
    x|=static_cast<u64>(input.ip)<<IP_OFFSET;
    x|=static_cast<u64>(input.weight)<<WEIGHT_OFFSET;
    x|=input.block;
    return {.a=input.serial,.b=x};
}

Codec::Output Codec::Decode(Codec::Data x) {
    return Output{}.load(x);
}

//int main(){
//    const char*ip="192.168.10.182";
////    const char*data="12@43@" BAD;
//    const char*data="12@179@-345";
//    Codec::Input input{};
//    input.parseIp(ip).parseWeight(data).parseBlock(data);
//    printf("%d,%d,%d,%d\n",input.error,input.ip,(short)input.weight,input.block);
//    printf("%s\n",Codec::Decode(Codec::Encode(input)).payload.c_str());
//}