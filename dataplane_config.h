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
        array<int, 2> hash_in_phv; // two ids of 32 bit container
        int depth;
        int key_width;
        int value_width;
        int match_field_byte_len;
        std::array<int, MAX_MATCH_FIELDS_BYTE_NUM> match_field_byte_ids;

        int default_action_id; // when match_table's key & value == 0, using default action id as action id; else default_action_id = -1;

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
            ACTION_DATA
        } type;
        union
        {
            uint32_t value;
            int phv_id;
            int action_data_id;
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

struct SALUnit
{
    int salu_id;
    enum OP
    {
        READ,
        WRITE,
        RAW,
        SUB,
        PRAW,
        IfElseRAW,
        NestedIf
    } op;

    struct Parameter
    {
        enum Type
        {
            CONST,
            HEADER,
            ACTION_DATA,
            REG,
        } type;
        enum IfType
        {
            COMPARE_EQ,
            EQ,
            NEQ,
            GT,
            LT,
            GTE,
            LTE
        } if_type;
        union
        {
            uint32_t value;
            int phv_id;
            int action_data_id;
            // 使用第几个带状态表的value
            int table_idx;
        } content;
        int value_idx;
    } left_value, operand1, operand2, operand3, return_value;

    struct ReturnValueFrom {
        enum Type
        {
            CONST,
            HEADER,
            ACTION_DATA,
            REG,
            OP1,
            OP2,
            OP3,
            LEFT
        } type, false_type;
        union
        {
            uint32_t value;
            int phv_id;
            int action_data_id;
            // 使用第几个带状态表的value
            int table_idx;
        } content, false_content;
        int value_idx, false_value_idx;
    } return_value_from;
};
std::array<SALUnit[MAX_SALU_NUM], PROCESSOR_NUM> SALUs;

// 每个 processor 中带状态表的数量, 最大为4

std::array<int, PROCESSOR_NUM> num_of_stateful_tables;

std::array<std::array<int, 4>, PROCESSOR_NUM> stateful_table_ids;

enum ProcType
{
    NONE,
    READ,
    WRITE
};
std::array<ProcType, PROCESSOR_NUM> proc_types;

std::array<int, PROCESSOR_NUM> read_proc_ids;
std::array<int, PROCESSOR_NUM> write_proc_ids;

std::array<std::array<int, 16>, PROCESSOR_NUM> state_idx_in_phv;

struct SavedState
{
    std::array<int, 4> saved_state_idx_in_phv;
    std::array<int, 4> state_lengths;
    int state_num;
};
std::array<SavedState, PROCESSOR_NUM> state_saved_idxs;

// every processor has up to 4 stateful tables, every stateful table have 64-bit hash value, need to be saved in two phvs
std::array<std::array<std::array<u32, 2>, 4> ,PROCESSOR_NUM> phv_id_to_save_hash_value;

// two positions in phv for flow id; can use any of the hash result
std::array<int, 2> flow_id_in_phv;

// 带状态表使用的salu序号
std::array<std::array<int, MAX_PARALLEL_MATCH_NUM>, PROCESSOR_NUM> salu_id; 

std::array<int, PROCESSOR_NUM> tags;

// initial increase clock
int backward_cycle_num;

int cycles_per_hb;

// for debug

const u64 DEBUG_FLOW_ID = 71498389351104549;

bool DEBUG_ENABLE = false;

#endif // RPISA_SW_DATAPLANE_CONFIG_H+
