//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_REGISTER_H
#define RPISA_SW_REGISTER_H

#include "../../defs.h"
#include "SRAM.h"

const int HASH_CYCLE = 4;
const int HASH_NUM = 16;
const int GATEWAY_NUM = 16;
const int ADDRESS_MAX_NUM = 2;
const int GET_KEY_ALL_CYCLE = HASH_CYCLE + 1 + 1;

struct GetKeyRegister {
    array<PHV, GET_KEY_ALL_CYCLE> phv;

    bool enable1;
    b1024 key;

    bool enable2;
    array<array<u32,  READ_TABLE_NUM>,    HASH_CYCLE>      hashValue;
    array<array<bool, READ_TABLE_NUM>,    HASH_CYCLE>     readEnable;
    array<array<b1024,  READ_TABLE_NUM>,    HASH_CYCLE>          keys;

};

const int MATCHER_ALL_CYCLE = 4;

struct MatcherRegister {

    array<PHV,  MATCHER_ALL_CYCLE> phv;

    array<u64,  READ_TABLE_NUM> hashValue;
    array<b1024,  READ_TABLE_NUM> keyCycleMatch;
    array<bool, READ_TABLE_NUM> readEnableCycleMatch;

    array<b1024,  READ_TABLE_NUM> valueMatchCycleCompare;
    array<b1024,  READ_TABLE_NUM> keyMatchCycleCompare;
    array<b1024,  READ_TABLE_NUM> keyCycleCompare;
    array<bool, READ_TABLE_NUM> readEnableCycleCompare;

    array<b1024,  READ_TABLE_NUM> valueCycleOutput;
    array<bool, READ_TABLE_NUM> compare;
    array<bool, READ_TABLE_NUM> readEnableOutput;


};

const int INSTRUCTION_NUM = 8;

using ALUInt = u32;
typedef ALUInt (*ALU)(ALUInt, ALUInt);
struct Instruction {

    ALU op;

    struct Parameter {
        enum Type {
            CONST, HEADER
        } type;
        union {
            ALUInt value;
            u32 id;
        } content ;
    } a, b;
    u32 id;
};

using VLIW = array<Instruction, INSTRUCTION_NUM>;


struct ExecutorRegister {

    PHV phvIF;
    array<bool, READ_TABLE_NUM> readEnable;
    array<bool, READ_TABLE_NUM> compare;
    array<b1024 , READ_TABLE_NUM> value;
    array<VLIW, READ_TABLE_NUM> vliw;


    PHV phvID;
    array<array<ALU, HEADER_NUM> , READ_TABLE_NUM> alu;
    array<array<pair<ALUInt, ALUInt>, HEADER_NUM>, READ_TABLE_NUM> parameter;
    array<array<bool, HEADER_NUM>, READ_TABLE_NUM> writeEnableEX;

    PHV phvEX;
    array<array<ALUInt , HEADER_NUM>, READ_TABLE_NUM> res;
    array<array<bool, HEADER_NUM>, READ_TABLE_NUM> writeEnableWB;

    PHV phvWB;


};



#endif //RPISA_SW_REGISTER_H
