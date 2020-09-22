//
// Created by Steve on 8/27/2020.
//

#include "secure.h"
#include "reader.h"

#include <cstring>

static RandomReader entropy{"/dev/urandom"};

using u8=Reader::u8;

typedef union{
    Reader::u32 x;
    Reader::u8 p[sizeof(Reader::u32)];
} IU;

//static FUNCTION_UNUSED void uniswap(u8*p){
//    IU u1{},u2{};
//    memcpy(u1.p,p,4);
//    memcpy(u2.p,p+8,4);
//    u1.x^=u2.x;
//    memcpy(p,u1.p,4);
//}

std::string Secure::Encrypt(const void *data, size_t len) {
    std::string res=SECRET;
    res.resize(kDataLen);
    memcpy(&res[SECRET_LEN+1],data,len); /*load real data*/
    auto p=reinterpret_cast<u8*>(&res[0]);
    u8 mask=entropy.fetch<u8>(); /*fetch random u8*/
    p[SECRET_LEN]=mask;
    size_t i=SECRET_LEN+1;
    for(;i!=SECRET_LEN;++i){
        if(i==kDataLen){
            i=0;
        }
        p[i]^=mask;
    }
//    uniswap(p); /*poor(lost effect)*/
    return res;
}

bool Secure::Decrypt(std::string &s, void *data, size_t len) {
    if(s.size()==kDataLen){
        auto p=reinterpret_cast<u8*>(&s[0]);
//        uniswap(p);
        auto mask=p[SECRET_LEN];
        size_t i=SECRET_LEN+1;
        for(;i!=SECRET_LEN;++i){
            if(i==kDataLen){
                i=0;
            }
            p[i]^=mask;
        }
        if(strncmp(&s[0],SECRET,SECRET_LEN)==0){
            memcpy(data,&s[SECRET_LEN+1],len);
            return true;
        }
    }
    return false;
}

//int main(){
//    long l=0x2345634523;
//    long r{};
//    auto s=Secure::Encrypt(&l,sizeof(l));
//    if(Secure::Decrypt(s,&r,sizeof(r))){
//        printf("%lx,%lx\n",l,r);
//    }else{
//        puts("UNKNOWN");
//    }
//    auto buf=Secure::CreateBuffer();
//    printf("buffer length: %ld\n",buf.size());
//    l=12;
//    s=Secure::Encrypt(&l,sizeof(l));
//    Secure::Decrypt(s,&r,sizeof(r));
//    printf("%lx,%lx\n",l,r);
//}