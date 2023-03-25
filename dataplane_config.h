//
// Created by root on 1/22/23.
//

#ifndef RPISA_SW_DATAPLANE_CONFIG_H
#define RPISA_SW_DATAPLANE_CONFIG_H

#include <unordered_map>
#include "PISA/Register/SRAM.h"

#define MAX_PHV_CONTAINER_NUM 224
#define MAX_MATCH_FIELDS_BYTE_NUM 128
#define MAX_GATEWAY_NUM 16
#define MAX_PARALLEL_MATCH_NUM 16
#define PROCESSOR_NUM 12
#define MAX_SALU_NUM 16

// store all configs that dataplane processors need: global variables;
// can only be configured before dataplane start

// getKey configs for N processors:
//      use which containers;
//      map containers to 1024bits
struct GetKeyConfig
{
    int used_container_num;

    struct UsedContainer2MatchFieldByte
    {
        int used_container_id;
        enum ContainerType
        {
            U8,
            U16,
            U32
        } container_type;
        std::array<int, 4> match_field_byte_ids;
    };

    std::array<UsedContainer2MatchFieldByte,
               MAX_MATCH_FIELDS_BYTE_NUM>
        used_container_2_match_field_byte;
};
GetKeyConfig getKeyConfigs[PROCESSOR_NUM];

// gateway configs for each processor:
//      use which 16 gateways;
//      ops
//      operands: from which match_fields
//      map result to match tables; map result to next gateways.

struct EnableFunctionsConfig{
    struct EnableFunction{
        bool enable;
        enum OP
        {
            EQ,
            GT,
            LT,
            GTE,
            LTE,
            NEQ
        } op;

        struct Parameter{
            enum Type{
                CONST,
                HEADER,
                STATE,
                REGISTER
            } type;
            // const value; id in match key; id of register
            u32 value;
        } operand1, operand2;

        u32 mask;
    };

    EnableFunction enable_functions[MAX_PARALLEL_MATCH_NUM][7];
};

EnableFunctionsConfig enable_functions_configs[PROC_NUM];

struct GatewaysConfig
{
    int processor_id;
    struct Gate
    {
        enum OP
        {
            EQ,
            GT,
            LT,
            GTE,
            LTE,
            NEQ
        } op;

        struct Parameter
        {
            enum Type
            {
                CONST,
                HEADER
            } type;
            union
            {
                uint32_t value;
                struct operand_match_field_byte
                {
                    int len;
                    std::array<int, 4> match_field_byte_ids;
                } operand_match_field_byte;
            } content;
        } operand1, operand2;
    };
    Gate gates[MAX_GATEWAY_NUM];

    // every processor has some masks, input key go through masks, and then searching the unordered_map
    std::vector<std::array<bool, MAX_PARALLEL_MATCH_NUM>> masks;
    std::unordered_map<uint32_t,
                       std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM>>
        gateway_res_2_match_tables;
    std::unordered_map<uint32_t,
                       std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM>>
        gateway_res_2_gates;
};
GatewaysConfig gatewaysConfigs[PROCESSOR_NUM];

std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;

// hash unit configs:
//      hash function

// table configs:
//      size: depth & width [key + value]
//      fields from which match_fields
//      number of hash ways
struct MatchTableConfig
{
    int processor_id;
    struct MatchTable
    {
        int type;        // 0: stateless table; 1: stateful table;
        int key_width;
        int value_width;
        int match_field_byte_len;
        std::array<int, MAX_MATCH_FIELDS_BYTE_NUM> match_field_byte_ids;

        int number_of_hash_ways; // stateful table 必定为 1
        int hash_bit_sum;

        std::array<int, 4> hash_bit_per_way;
        std::array<int, 4> srams_per_hash_way;
        std::array<std::array<int, 80>, 4> key_sram_index_per_hash_way;
        std::array<std::array<int, 80>, 4> value_sram_index_per_hash_way;
    };
    int match_table_num;
    MatchTable matchTables[MAX_PARALLEL_MATCH_NUM];
};
MatchTableConfig matchTableConfigs[PROCESSOR_NUM];

struct EFSMTableConfig{
    struct EFSMTable{
        int key_width;
        int value_width;
        int byte_len;
        int default_action_id;
        int num_of_hash_ways;
        int hash_bit_sum;

        std::array<int, 4> hash_bit_per_way;
        std::array<int, 4> srams_per_hash_way;
        std::array<std::array<int, 80>, 4> key_sram_index_per_hash_way;
        std::array<std::array<int, 80>, 4> mask_sram_index_per_hash_way;
        std::array<std::array<int, 80>, 4> value_sram_index_per_hash_way;
    };
    int efsm_table_num;
    EFSMTable efsmTables[MAX_PARALLEL_MATCH_NUM];
};

EFSMTableConfig efsmTableConfigs[PROCESSOR_NUM];

std::array<std::array<SRAM, SRAM_NUM>, PROCESSOR_NUM> SRAMs;

// action configs:
//      action_id to primitive mapping: enable which way of 224
//      ALU: alu_id and op;
//      ALU operands: from PHV (field & metadata), from constant registers, from action_data
//      action_data: extract from value, action_data_id

struct ALUnit
{
    int alu_id;

    enum OP
    {
        SET,
        PLUS,
        NONE
    } op;

    struct Parameter
    {
        enum Type
        {
            CONST,
            HEADER,
            ACTION_DATA,
            STATE,
            REGISTER
        } type;
        union
        {
            uint32_t value;
            int phv_id;
            int action_data_id;
            int table_id;
            struct{
                int table_id;
                int register_id;
            } table_register_id;
        } content;
    } operand1, operand2;
};
std::array<ALUnit[MAX_PHV_CONTAINER_NUM], PROCESSOR_NUM> ALUs;

struct ActionConfig
{
    int processor_id;

    struct ActionData
    {
        int data_id;
        int byte_len;
        int bit_len;
        u32 value;
    };

    struct Action
    {
        int action_id;
        int action_data_num;
        std::array<bool, MAX_PHV_CONTAINER_NUM> vliw_enabler;
        std::array<ALUnit, MAX_PHV_CONTAINER_NUM> alu_configs;
    };
    Action actions[16];
};
ActionConfig actionConfigs[PROCESSOR_NUM];


// two positions in phv for flow id; can use any of the hash result
std::array<int, 2> flow_id_in_phv;
// for debug

const u64 DEBUG_FLOW_ID = 256705178760184036;

bool DEBUG_ENABLE = false;

#endif // RPISA_SW_DATAPLANE_CONFIG_H+
