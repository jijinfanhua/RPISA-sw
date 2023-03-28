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

struct DispatcherQueueItem{
    PHV phv;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
};

struct HashRegister : public BaseRegister{
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
};

struct DispatcherRegister: public BaseRegister{
    std::array<u32, MAX_MATCH_FIELDS_BYTE_NUM> key;
    DispatcherQueueItem dq_item;
};

struct GetAddressRegister : public BaseRegister {
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
};

struct MatchRegister : public BaseRegister {
    // 16 matches -> every match has 4 maximum hash ways -> each hash way may have 1024bit (8 * 128(sram_width))
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> key_sram_columns;
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> on_chip_addrs;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    // caution: hash_value should be transferred to action, becase it should serve the SALU
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
};

struct CompareRegister : public BaseRegister {
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;

    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_values;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
};

struct ConditionEvaluationRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<bool, MAX_PARALLEL_MATCH_NUM> hits;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct KeyRefactorRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    array<array<bool, 7>, MAX_PARALLEL_MATCH_NUM> enable_function_result;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
};

struct EfsmHashRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys; // refactored match table keys
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct EfsmGetAddressRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct EfsmMatchRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> key_sram_columns;
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> value_sram_columns;
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> mask_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> on_chip_addrs;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct EfsmCompareRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_keys;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_masks;
    std::array<std::array<std::array<b128, 8>, 4>, MAX_PARALLEL_MATCH_NUM> obtained_values;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct GetActionRegister : public BaseRegister {
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<std::pair<std::array<b128, 8>, bool>, MAX_PARALLEL_MATCH_NUM> final_values;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct ExecuteActionRegister : public BaseRegister {
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;

    std::array<ActionConfig::ActionData, 16> action_data_set;
    std::array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> vliw_enabler;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> states;
    std::array<std::array<u32, 16>, MAX_PARALLEL_MATCH_NUM> registers;
};

struct UpdateStateRegister: public BaseRegister{
    std::array<std::array<std::array<int, 8>, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_value_sram_columns;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> flow_context_on_chip_addrs;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_matched_way;
    std::array<u32, MAX_PARALLEL_MATCH_NUM> flow_context_empty_way;
};




#endif //RPISA_SW_REGISTER_H
