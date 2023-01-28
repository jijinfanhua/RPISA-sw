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

// store all configs that dataplane processors need: global variables;
// can only be configured before dataplane start

// getKey configs for N processors:
//      use which containers;
//      map containers to 1024bits
struct GetKeyConfig {
    int processor_id;
    int used_container_num;

    struct UsedContainer2MatchFieldByte {
        int used_container_id;
        enum ContainerType{
            U8, U16, U32
        } container_type;
        std::array<int, 4> match_field_byte_ids;
    };

    std::array<UsedContainer2MatchFieldByte,
            MAX_MATCH_FIELDS_BYTE_NUM> used_container_2_match_field_byte;
};
GetKeyConfig getKeyConfigs[PROCESSOR_NUM];

// gateway configs for each processor:
//      use which 16 gateways;
//      ops
//      operands: from which match_fields
//      map result to match tables; map result to next gateways.
struct GatewaysConfig {
    int processor_id;
    struct Gate {
        enum OP {
            EQ, GT, LT, GTE, LTE, NONE
        } op;

        struct Parameter {
            enum Type {CONST, HEADER} type;
            union {
                uint32_t value;
                struct operand_match_field_byte {
                    int len;
                    std::array<int, 4> match_field_byte_ids;
                } operand_match_field_byte;
            } content;
        } operand1, operand2;
    };
    Gate gates[MAX_GATEWAY_NUM];
    std::unordered_map<uint32_t,
        std::array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM>> gateway_res_2_match_tables;
    std::unordered_map<uint32_t,
            std::array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM>> gateway_res_2_gates;
};
GatewaysConfig gatewaysConfigs[PROCESSOR_NUM];

// hash unit configs:
//      hash function

// table configs:
//      size: depth & width [key + value]
//      fields from which match_fields
//      number of hash ways
struct MatchTableConfig {
    int processor_id;
    struct MatchTable {
        int depth;
        int key_width;
        int value_width;
        int match_field_byte_len;
        std::array<int, MAX_MATCH_FIELDS_BYTE_NUM> match_field_byte_ids;

        int number_of_hash_ways;
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

std::array<SRAM, SRAM_NUM> SRAMs;

// action configs:
//      action_id to primitive mapping: enable which way of 224
//      ALU: alu_id and op;
//      ALU operands: from PHV (field & metadata), from constant registers, from action_data
//      action_data: extract from value, action_data_id
struct ActionConfig {
    int processor_id;

    struct ALU {
        int alu_id;

        enum OP {
            SET, PLUS, NONE
        } op;

        struct Parameter {
            enum Type {CONST, HEADER, ACTION_DATA} type;
            union {
                uint32_t value;
                int phv_id;
                int action_data_id;
            } content;
        } operand1, operand2;
    };

    struct Action {
        int action_id;
        struct ActionData {
            int data_id;
            int byte_len;
            int bit_len;
            u32 value;
        };
        ActionData actionDatas[16];
        int action_data_num;
        std::array<bool, MAX_PHV_CONTAINER_NUM> vliw_enabler;
        std::array<ALU, MAX_PHV_CONTAINER_NUM> alus;
    };
    Action actions[4];
};
ActionConfig actionConfigs[PROCESSOR_NUM];

#endif //RPISA_SW_DATAPLANE_CONFIG_H
