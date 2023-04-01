//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_DEFS_H
#define RPISA_SW_DEFS_H

#include <list>
#include <array>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>

using namespace std;


const int KEY_LENGTH = 1024;
const int KEY_NUM = 32;
const int VALUE_NUM = 4;
const int HEADER_NUM = 224;
const int BUFFER_SIZE = 32;
const int READ_TABLE_NUM = 16;
const int SRAM_NUM = 80;

const int SRAM_SIZE = 1024;
const int LOG_SRAM_SIZE = 10;

const int TAG_IN_PHV = 222;
const int ID_IN_PHV = 223;


struct PipeLine;
using u32 = unsigned int;
using u64 = unsigned long long;
using b1024 = std::array<u32, KEY_NUM>;
using b128 = std::array<u32, VALUE_NUM>;
using PHV = std::array<u32, HEADER_NUM>; // 这里8位，16位，32位都是使用u32存的，如果真实是8，那当他是8位的就好

// 这里分成不同的逻辑段来进行实现，他们都应继承Logic，具体的实现应该写在别的文件中
struct GetKey   ;
struct Gateway  ;
struct GetHash  ;
struct GetAddress;
struct Matches;
struct Compare;

struct GetAction;
struct ExecuteAction;

struct VerifyStateChange;
struct PIW;
struct PIR;
struct PO;
struct RI;
struct RO;
struct PIR_asyn;

struct SRAM;



struct PORegister        ;
struct P2RElementRegister;
struct R2PElementRegister;
struct PacketInfo;
struct PIWRegister       ;
struct MatcherRegister   ;
struct ExecutorRegister  ;
struct RIRegister        ;
struct RORegister        ;
struct PipeLine          ;



const int TimeOutSettingErrorException = 1;

u64 u32_to_u64(u32 high, u32 low)
{
    u64 high_ = (u64)high;
    u64 result = (high_ << 32) + low;
    return result;
}

u64 u16_array_to_u64(std::array<u32, 4> input)
{
    // input from high to low
    u64 first16 = input[0] << 16 >> 16;
    u64 second16 = input[1] << 16 >> 16;
    u64 third16 = input[2] << 16 >> 16;
    u64 fourth16 = input[3] << 16 >> 16;
    return (first16 << 48) + (second16 << 32) + (third16 << 16) + fourth16;
}

std::array<u32, 4> u64_to_u16_array(u64 input)
{
    std::array<u32, 4> output{};
    output[0] = input >> 48;
    output[1] = input << 16 >> 48;
    output[2] = input << 32 >> 48;
    output[3] = input << 48 >> 48;
    return output;
}


#endif //RPISA_SW_DEFS_H
