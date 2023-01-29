//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_RPISA_REGISTER_H
#define RPISA_SW_RPISA_REGISTER_H

#include "../../defs.h"

/*********** fengyong add start ***************/
//struct RingRegister {
//    std::array<bool, PROC_NUM> dst1;
//    u32 ctrl;
//    std::array<bool, PROC_NUM> dst2;
//    u32 addr;
//    std::array<u32, 128> payload;
//};

/*********** fengyong add end ***************/

struct PacketInfo {
    HeartBeat heartBeat;
    int destProcessor;
    enum Type {
        ONLY_HEARTBEAT, CANCEL_DIRTY, WRITE, BP
    };
    Type type;
    // todo: 还需要一个state
    PHV phv;
    u32 label;

    // 一路就够了
    array<b1024,  READ_TABLE_NUM> key;
    array<u64,  READ_TABLE_NUM> hashValue;
    array<bool, READ_TABLE_NUM> readEnable;
    PacketInfo(
            int proc_id,
            const PHV& phv,
            const array<b1024,  READ_TABLE_NUM>& key,
            const array<u32,  READ_TABLE_NUM>& hashValue,
            const array<bool, READ_TABLE_NUM>& readEnable) :
            phv(phv), key(key), hashValue(hashValue), readEnable(readEnable) {
        label = hashValue[0];
        type = BP;
        heartBeat = {};
        heartBeat[proc_id] = true;

    }
    PacketInfo(int proc_id, u32 label, Type type): label(label), type(type) {
        heartBeat = {};
        heartBeat[proc_id] = true;
    }

    PacketInfo() = default;

};

using DirtyTable = map<int, bool>;




struct PIRegister {

    bool isRead;
    PacketInfo output;
    bool priority;

    bool isDirty;
    PacketInfo packet;


};


struct PORegister {

};

struct RORegister {
    PacketInfo output;
};

struct RIRegister {
    PacketInfo input;
};

#endif
