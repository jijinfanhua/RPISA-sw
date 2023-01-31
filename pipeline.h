//
// Created by root on 1/28/23.
//

#ifndef RPISA_SW_PIPELINE_H
#define RPISA_SW_PIPELINE_H

#include "PISA/Register/Register.h"
#include "RPISA/Register/RRegister.h"
// 这里记录了所有的有关于时序的信息，包括buffer的状态，各种流水的走法等
// 每一个Register应自己管理好自己所经营范围内的phv流水线

struct ProcessorRegister {

    Buffer<RingRegister> rp2p;
    queue<RingRegister> p2r;
    queue<RingRegister> r2r;

    /****** fengyong add begin ********/
    BaseRegister base;
    GetKeysRegister getKeys;
    GatewayRegister gateway[2];
    HashRegister hashes[4];
    GetAddressRegister getAddress;
    MatchRegister match;
    CompareRegister compare;
    GetActionRegister getAction;
    ExecuteActionRegister executeAction;
    /****** fengyong add end *********/

    PIRegister pi;
    PORegister po;
    RIRegister ri;
    RORegister ro;
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
