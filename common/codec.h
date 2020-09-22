//
// Created by Steve on 8/26/2020.
//

#ifndef C4FUN_CODEC_H
#define C4FUN_CODEC_H

#include <string>

#include "tool.h"

#define ERROR_OFFSET 56u
#define IP_OFFSET 48u
#define WEIGHT_OFFSET 32u
#define IP_MASK 0xffu
#define WEIGHT_MASK 0xffffu
#define BLOCK_MASK 0xffffffffu
//#define WEIGHT_COMPLEMENT 0x10000u

class Codec{
public:
    using u8=unsigned char;
    using u16=unsigned short;
    using u32=unsigned int;
    using u64=unsigned long;

    struct Data{
        u32 a;
        u64 b;
    } TYPE_PACKED;

    static constexpr const size_t kDataSize=sizeof(Data);

    struct Input{
        u8 error{0};
        u8 ip{0};
        u16 weight{0};
        u32 block{0};
        u32 serial{0};
        Input&parseIp(const char*data);
        Input&parseWeight(const char*data);
        Input&parseBlock(const char*data);
        Input&parseSerial(const char*data);
    };
    struct Output{
        std::string payload;
        std::string topic;
        Output&load(Data data);
        static std::string RetrieveTopic(u64 x){
            return std::to_string(static_cast<u8>(x>>IP_OFFSET));
        }
    };

public:
    static Data Encode(const Input&input);
    static Output Decode(Data x);
};

#endif //C4FUN_CODEC_H
