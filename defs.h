//
// Created by Walker on 2023/1/12.
//

#ifndef RPISA_SW_DEFS_H
#define RPISA_SW_DEFS_H

#include <list>
#include <vector>
#include <queue>

#include <unordered_map>

using namespace std;


const int KEY_LENGTH = 1024;
const int KEY_NUM = 32;
const int HEADER_NUM = 224;
const int PROC_NUM = 16;
const int BUFFER_SIZE = 32;

struct PipeLine;
using u32 = unsigned int;
using u64 = unsigned long long;
using Key = array<u32, KEY_NUM>;
using PHV = array<u32, HEADER_NUM>; // 这里8位，16位，32位都是使用u32存的，如果真实是8，那当他是8位的就好

struct Logic {

    // 每一个Logic都应标识自己所在的processor id
    int processor_id;

    Logic(int processor_id) : processor_id(processor_id) {}

    // 每一个Logic都应实现这个时序算法，在pipeline中找到自己应该操控的寄存器，
    // now表示当前时钟的状态，通过对next中的寄存器赋值，来实现对下一个时钟的操作
    // 说起来简单，但务必注意，所有的时序寄存器都在PipeLine的Register结构体中，与Logic是割离的
    // Logic中不应存有任何与时序有关的Register的值，而只负责组合行为和必要配置
    virtual void execute(const PipeLine& now, PipeLine& next) = 0;

};


// 这里分成不同的逻辑段来进行实现，他们都应继承Logic，具体的实现应该写在别的文件中
struct GetKey   ;
struct PIR      ;
struct PO       ;
struct Matcher  ;
struct Executor ;
struct PIW      ;
struct RI       ;
struct RO       ;


struct Processor;
struct SRAM;

// 这里记录了所有的有关于时序的信息，包括buffer的状态，各种流水的走法等
// 每一个Register应自己管理好自己所经营范围内的phv流水线
struct PipeLine {



    /**
     * Buffer这里我打算采用最简单的设计，即使用queue来标识buffer长度，然后通过
     * log函数，来实时监控各个buffer的长度状态，必要时可以输出csv文件供后续交给python做数据处理
     */
    struct Buffer {
        array<queue<PHV>, BUFFER_SIZE> q;
        void Log();
    };

    struct GetKeyRegister    ;
    struct PIRegister        {
        Buffer p2r;
    };
    struct PIRRegister : public PIRegister      {};
    struct PIWRegister : public PIRegister      {};
    struct MatcherRegister   {};
    struct ExecutorRegister  {};
    struct RIRegister        {};
    struct RORegister        {};

    struct ProcessorRegister {

        Buffer r2p, p2p;
        GetKeyRegister getKey;
        PIRRegister pir;
        PIWRegister piw;
        MatcherRegister matcher;
        ExecutorRegister executor;

    };

    struct RingRegister {
        struct RI {};
        struct RO {};
    };

    // And god said, let here be processor, and here was processor.
    array<ProcessorRegister, PROC_NUM> processors;

    PipeLine execute() {
        PipeLine next;
        // 这里还没完善，就是要调用所有Processor的所有Logic，走完一次时钟
        // 这里因为Logic都还没写，所以没有进行具体的实现
        return next;
    }

    void log();

};


#endif //RPISA_SW_DEFS_H
