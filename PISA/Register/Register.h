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
    PHV phv;

    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
};

struct GetKeysRegister : public BaseRegister{

};
#define GATEWAY_CYCLE_NUM 2
struct GatewayRegister : public BaseRegister {
    array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<bool, MAX_GATEWAY_NUM> gate_res;
};

struct HashRegister : public BaseRegister{
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
};

struct GetAddressRegister : public BaseRegister {
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    bool backward_pkt;
};

struct MatchRegister : public BaseRegister {
    // 16 matches -> every match has 4 maximum hash ways -> each hash way may have 1024bit (8 * 128(sram_width))
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> key_sram_columns;
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> on_chip_addrs;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    // caution: hash_value should be transferred to action, becase it should serve the SALU
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    bool backward_pkt;
};

struct CompareRegister : public BaseRegister {
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_values;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    bool backward_pkt;
};

struct GetActionRegister : public BaseRegister {
    std::array<bool, 16> compare_result_for_salu;
    std::array<u32, 16> obtained_value_for_salu;
    std::array<std::pair<std::array<b128, 8>, bool>, MAX_PARALLEL_MATCH_NUM> final_values;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
};

struct ExecuteActionRegister : public BaseRegister {
    std::array<ActionConfig::ActionData, 16> action_data_set;
    std::array<bool, 16> compare_result_for_salu;
    std::array<u32, 16> obtained_value_for_salu;
    std::array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> vliw_enabler;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
};

struct VerifyStateChangeRegister : public BaseRegister {
    std::array<bool, 224> phv_changed_tags = {false};
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
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
