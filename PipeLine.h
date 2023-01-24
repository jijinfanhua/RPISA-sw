//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_PIPELINE_H
#define RPISA_SW_PIPELINE_H

#include "PISA/Register/Register.h"
#include "RPISA/Register/Register.h"
// 这里记录了所有的有关于时序的信息，包括buffer的状态，各种流水的走法等
// 每一个Register应自己管理好自己所经营范围内的phv流水线

struct ProcessorRegister {

    Buffer<PacketInfo> p2p, r2p;
    queue<PacketInfo> p2r;
    queue<PacketInfo> r2r;
    GetKeyRegister getKey;

    /****** fengyong add begine ********/
    GetKeysRegister getKeys;
    GatewayRegister gateway[2];
    HashRegister hash;
    GetAddressRegister getAddress;
    /****** fengyong add end *********/

    PIRegister pi;
    PORegister po;
    RIRegister ri;
    RORegister ro;

    MatcherRegister matcher;
    ExecutorRegister executor;


};


struct PipeLine {

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


#endif //RPISA_SW_PIPELINE_H
