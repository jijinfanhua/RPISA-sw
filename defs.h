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
const int PROC_NUM = 16;
const int BUFFER_SIZE = 32;
const int READ_TABLE_NUM = 16;
const int SRAM_NUM = 80;

const int SRAM_SIZE = 1024;
const int LOG_SRAM_SIZE = 10;


struct PipeLine;
using u32 = unsigned int;
using u64 = unsigned long long;
using b1024 = array<u32, KEY_NUM>;
using b128 = array<u32, VALUE_NUM>;
using PHV = array<u32, HEADER_NUM>; // 这里8位，16位，32位都是使用u32存的，如果真实是8，那当他是8位的就好


using HeartBeat = array<bool, PROC_NUM>;


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

/**
 * Buffer这里我打算采用最简单的设计，即使用queue来标识buffer长度，然后通过
 * log函数，来实时监控各个buffer的长度状态，必要时可以输出csv文件供后续交给python做数据处理
 */
template< class T>
struct Buffer {
    map<int, queue<T>> q;
    void Log();
    bool contain(int key) const {
        return q.find(key) != q.end() && !q.at(key).empty();
    }
    void push(int key, const T& t) {
        q[key].push(t);
    }
    T pop(int key) {
        T res = q[key].front();
        q[key].pop();
        return res;
    }

};




struct GetKeyRegister    ;
struct PIRRegister       ;
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



#include "util/BitArray.h"

#endif //RPISA_SW_DEFS_H
