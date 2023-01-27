//
// Created by ASUS on 2023/1/12.
//

#ifndef RPISA_SW_REGISTER_H
#define RPISA_SW_REGISTER_H

#include "../../defs.h"
#include "SRAM.h"
#include "../../dataplane_config.h"

const int HASH_CYCLE = 4;
const int ADDRESS_WAY = 4;
const int HASH_NUM = 16;
const int GATEWAY_NUM = 16;
const int ADDRESS_MAX_NUM = 2;
const int GET_KEY_ALL_CYCLE = HASH_CYCLE + 1 + 1;

/*********** fengyong add start ***************/
struct BaseRegister {
    bool enable1;
    array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;

    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;
};

struct GetKeysRegister : public BaseRegister{
    array<PHV, GET_KEY_ALL_CYCLE> phv;

//    bool enable1;
//    array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
//
//    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
//    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;


//    bool enable2;
};
#define GATEWAY_CYCLE_NUM 2
struct GatewayRegister {
//    struct Gateway_Per_Cycle {
    PHV phv;
    bool enable1;
    array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<bool, MAX_GATEWAY_NUM> gate_res;
    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;
//    };
//    gate_cycles[GATEWAY_CYCLE_NUM];
};

struct HashRegister {
    PHV phv;
    bool enable1;

    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;

    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;
};

struct GetAddressRegister {
    PHV phv;
    bool enable1;

    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;

    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;
};

struct MatchRegister {
    PHV phv;
    bool enable1;

    std::array<std::array<int, 8>, MAX_PARALLEL_MATCH_NUM> sram_columns;
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM> gateway_guider;
};
/*********** fengyong add end ***************/

struct GetKeyRegister {
    array<PHV, GET_KEY_ALL_CYCLE> phv;

    bool enable1;
    b1024 key;

    bool enable2;
    array<array<u32,  READ_TABLE_NUM>,    HASH_CYCLE>      hashValue;
    array<array<bool, READ_TABLE_NUM>,    HASH_CYCLE>     readEnable;

    array<array<u32, ADDRESS_WAY>,    READ_TABLE_NUM>        address;

};

const int MATCHER_ALL_CYCLE = 4;

struct MatcherRegister {

    array<PHV, MATCHER_ALL_CYCLE> phv;

    array<array<u32,   ADDRESS_WAY>,  READ_TABLE_NUM>        addressCycleMatch;
    array<b1024,                      READ_TABLE_NUM>            keyCycleMatch;
    array<bool,                       READ_TABLE_NUM>     readEnableCycleMatch;

    array<array<b1024, ADDRESS_WAY>,  READ_TABLE_NUM>   valueMatchCycleCompare;
    array<array<b1024, ADDRESS_WAY>,  READ_TABLE_NUM>     keyMatchCycleCompare;
    array<b1024,  READ_TABLE_NUM>        keyCycleCompare;
    array<bool,   READ_TABLE_NUM> readEnableCycleCompare;

    array<array<b1024,  READ_TABLE_NUM>, MATCHER_ALL_CYCLE - 1> valueCycleOutput;
    array<array<bool,   READ_TABLE_NUM>, MATCHER_ALL_CYCLE - 1> compare;
    array<array<bool,   READ_TABLE_NUM>, MATCHER_ALL_CYCLE - 1> readEnableOutput;


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
    array<bool,   READ_TABLE_NUM> readEnable;
    array<bool,   READ_TABLE_NUM> compare;
    array<b1024 , READ_TABLE_NUM> value;
    array<VLIW,   READ_TABLE_NUM> vliw;


    PHV phvID;
    array<array<ALU,                  HEADER_NUM>, READ_TABLE_NUM> alu;
    array<array<pair<ALUInt, ALUInt>, HEADER_NUM>, READ_TABLE_NUM> parameter;
    array<array<bool,                 HEADER_NUM>, READ_TABLE_NUM> writeEnableEX;

    PHV phvEX;
    array<array<ALUInt ,              HEADER_NUM>, READ_TABLE_NUM> res;
    array<array<bool,                 HEADER_NUM>, READ_TABLE_NUM> writeEnableWB;

    PHV phvWB;


};



#endif //RPISA_SW_REGISTER_H
