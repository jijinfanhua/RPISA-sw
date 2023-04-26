//
// Created by Walker on 2023/1/29.
//

#ifndef RPISA_SW_SWITCH_H
#define RPISA_SW_SWITCH_H

#include "Parser.h"
#include "../pipeline.h"
#include "../PISA/match.h"
#include "../dataplane_config.h"
#include "../PISA/action.h"
#include "../RPISA/Schedule.h"

#include <memory>
#include <string>
const int PACKET_SIZE_MAX = 1600;

struct ProcessorConfig
{

    GetKeyConfig getKeyConfig{};
    GatewaysConfig gatewaysConfig{};
    MatchTableConfig matchTableConfig{};
    ActionConfig actionConfig{};

    int processor_id;

    explicit ProcessorConfig(int id) : processor_id(id)
    {
        getKeyConfig.used_container_num = 0;
        gatewaysConfig.processor_id = id;
        matchTableConfig.processor_id = id;
        matchTableConfig.match_table_num = 0;
        actionConfig.processor_id = id;

        gatewaysConfig.masks.clear();
        gatewaysConfig.keys.clear();
        gatewaysConfig.values.clear();
        gatewaysConfig.gateway_res_2_gates.clear();
    }

    void push_back_get_key_use_container(
        int id, int size,
        int match_field1,
        int match_field2 = -1,
        int match_field3 = -1,
        int match_field4 = -1)
    {
        int k = getKeyConfig.used_container_num;
        GetKeyConfig::UsedContainer2MatchFieldByte x;
        x.match_field_byte_ids[0] = match_field1;
        x.match_field_byte_ids[1] = match_field2;
        x.match_field_byte_ids[2] = match_field3;
        x.match_field_byte_ids[3] = match_field4;
        x.used_container_id = id;

        switch (size)
        {
        case 8:
        {
            x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U8;
            for (int i = 0; i < 1; i++)
            {
                if (x.match_field_byte_ids[i] == -1)
                {
                    throw "match field " + to_string(k) + "id error or insufficient";
                }
            }
            break;
        }
        case 16:
        {
            x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U16;
            for (int i = 0; i < 2; i++)
            {
                if (x.match_field_byte_ids[i] == -1)
                {
                    throw "match field " + to_string(k) + "id error or insufficient";
                }
            }
            break;
        }
        case 32:
        {
            x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U32;
            for (int i = 0; i < 4; i++)
            {
                if (x.match_field_byte_ids[i] == -1)
                {
                    throw "match field " + to_string(k) + "id error or insufficient";
                }
            }
            break;
        }
        default:
            throw "container size error! must in 8, 16, 32!";
        }
        getKeyConfig.used_container_2_match_field_byte[k] = x;
        getKeyConfig.used_container_num++;
    }



private:
    static u32 bool_array_2_u32(const array<bool, GATEWAY_NUM> bool_array)
    {
        u32 sum = 0;
        for (int i = 0; i < 16; i++)
        {
            if(bool_array[i]){
                sum += (1 << (15 - i));
            }
        }
        return sum;
    }

public:

   void insert_gateway_entry(std::array<bool, GATEWAY_NUM> mask, std::array<bool, GATEWAY_NUM> key, std::array<bool, GATEWAY_NUM> value){
        gatewaysConfig.masks.push_back(mask);
        std::array<bool, PROCESSOR_NUM*GATEWAY_NUM> v{};
        std::copy(value.begin(), value.end(), v.begin() + processor_id*GATEWAY_NUM);
        gatewaysConfig.values.push_back(v);
        gatewaysConfig.keys.push_back(bool_array_2_u32(key));
    }

    void set_gateway_enable(const std::vector<int>& enable_id) const{
        for(auto i: enable_id){
            gateway_guider[GATEWAY_NUM*processor_id+i] = true;
        }
    }

    void commit() const
    {
        getKeyConfigs[processor_id] = getKeyConfig;
        gatewaysConfigs[processor_id] = gatewaysConfig;
        matchTableConfigs[processor_id] = matchTableConfig;
        actionConfigs[processor_id] = actionConfig;
    }
};

void commit(const vector<ProcessorConfig> &config)
{
    for (int i = 0; i < config.size(); i++)
    {
        config[i].commit();
    }
}

void top_heavy_hitter_config2(){
    flow_id_in_phv = {166, 167};
    gateway_guider[48] = gateway_guider[49] = gateway_guider[50] = gateway_guider[51] = gateway_guider[52] = 1;

    ProcessorConfig proc0 = ProcessorConfig(0);
    proc0.push_back_get_key_use_container(0, 8, 0);
    proc0.push_back_get_key_use_container(64, 16, 1, 2);
    proc0.push_back_get_key_use_container(65, 16, 3, 4);
    proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
    proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

    proc0.insert_gateway_entry({false}, {false}, {true, true, true});


    proc0.matchTableConfig.match_table_num = 3;
    auto& config_0 = proc0.matchTableConfig.matchTables[0];
    config_0.config_match_table(
        1, {166, 167},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4,
        40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{0},{2},{4},{6}}},
        {{{1},{3},{5},{7}}}
    );


    auto&  config_1 = proc0.matchTableConfig.matchTables[1];
    config_1.config_match_table(
        1, {168, 169},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{8},{10},{12},{14}}},
        {{{9},{11},{13},{15}}}
    );

    auto& config_2 = proc0.matchTableConfig.matchTables[2];
    config_2.config_match_table(
        1, {170, 171},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{16},{18},{20},{22}}},
        {{{17},{19},{21},{23}}}
    );

    array<bool, 224> vliw_enabler = {0};
    proc0.commit();

    auto& salu = SALUs[0][0];
    salu.config(
        224,
        SALUnit::IfElseRAW,
        //left value
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int)0, 3,
        //operand1
        SALUnit::Parameter::Type::CONST, (int)0, 0,
        //operand2
        SALUnit::Parameter::Type::CONST, (uint32_t)1, 0,
        //operand3
        SALUnit::Parameter::Type::CONST, (uint32_t)0, 0,
        //return value
        SALUnit::Parameter::Type::HEADER, (int)162,
        //return value from
        SALUnit::ReturnValueFrom::Type::REG, (int)0, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (uint32_t)0, 0
    );

    auto& salu1 = SALUs[0][1];
    salu1.config(
        225,
        SALUnit::IfElseRAW,
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int)1, 3,
        SALUnit::Parameter::Type::CONST, (uint32_t)0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t)1, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t)0, 0,
        SALUnit::Parameter::Type::HEADER,(int)163,
        SALUnit::ReturnValueFrom::Type::REG, (int)1, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (uint32_t)0, 0
    );

    auto& salu2 = SALUs[0][2];
    salu2.config(
        226,
        SALUnit::IfElseRAW,
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int)2, 3,
        SALUnit::Parameter::Type::CONST, (uint32_t)0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t)1, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t)0, 0,
        SALUnit::Parameter::Type::HEADER, (int)164,
        SALUnit::ReturnValueFrom::Type::REG, (int)2, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (uint32_t)0, 0
    );

    num_of_stateful_tables[0] = 3;
    stateful_table_ids[0] = {0, 1, 2};
    proc_types[0] = ProcType::READ;
    state_idx_in_phv[0] = {162, 163, 164};

    auto& phv_id_to_save_hash_value_0 = phv_id_to_save_hash_value[0];
    phv_id_to_save_hash_value_0[0] = {166, 167};
    phv_id_to_save_hash_value_0[1] = {168, 169};
    phv_id_to_save_hash_value_0[2] = {170, 171};

    auto& salu_id_0 = salu_id[0];
    salu_id_0 = {224, 225, 226};

    auto& saved_state_0 = state_saved_idxs[0];
    saved_state_0.config(3, {162, 163, 164}, {1, 1, 1});
    // processor1 finished

    ProcessorConfig proc1 = ProcessorConfig(2);

    proc1.matchTableConfig.match_table_num = 2;
    auto& match_table_1 = proc1.matchTableConfig.matchTables[0];
    match_table_1.type = 1;
    auto& match_table_2 = proc1.matchTableConfig.matchTables[1];
    match_table_2.type = 1;
    proc1.commit();

    num_of_stateful_tables[2] = 2;
    stateful_table_ids[2] = {0, 1};
    auto& phv_id_to_save_hash_value_1 = phv_id_to_save_hash_value[2];
    phv_id_to_save_hash_value_1[0] = {172, 173};
    phv_id_to_save_hash_value_1[1] = {172, 173};

    auto& salu_id_1 = salu_id[2];
    salu_id_1 = {224, 225};
    // if phv_162 >= phv_163, phv_165 = phv_162; else: phv_165 = phv_163
    auto& salu3 = SALUs[2][0];
    salu3.config(
        224,
        SALUnit::OP::IfElseRAW,
        SALUnit::Parameter::Type::HEADER,
        SALUnit::Parameter::IfType::GTE,
        (int) 162, 0,
        SALUnit::Parameter::Type::HEADER, (int) 163, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 165,
        SALUnit::ReturnValueFrom::Type::LEFT, (uint32_t)0, 0,
        SALUnit::ReturnValueFrom::Type::OP1, (uint32_t)0, 0
    );


    // if phv_162 >= phv_163, phv_1 = 0; else: phv_1 = 1;
    auto& salu4 = SALUs[2][1];
    // processor_2 finished, using salu instead of normal alu
    salu4.config(
        225,
        SALUnit::OP::IfElseRAW,
        SALUnit::Parameter::Type::HEADER,
        SALUnit::Parameter::IfType::GTE,
        (int) 162, 0,
        SALUnit::Parameter::Type::HEADER, (int) 163, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 1,
        SALUnit::ReturnValueFrom::CONST, (uint32_t) 0, 0,
        SALUnit::ReturnValueFrom::CONST, (uint32_t) 1, 0
    );


    ProcessorConfig proc2 = ProcessorConfig(3);
    proc2.push_back_get_key_use_container(164, 32, 0, 1, 2, 3);
    proc2.push_back_get_key_use_container(165, 32, 4, 5, 6, 7);
    proc2.push_back_get_key_use_container(1, 8, 8);

    auto& gate1 = proc2.gatewaysConfig.gates[0];
    gate1.config(
        GatewaysConfig::Gate::OP::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}}
    );

    auto& gate2 = proc2.gatewaysConfig.gates[1];
    gate2.config(
        GatewaysConfig::Gate::OP::NEQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    auto& gate3 = proc2.gatewaysConfig.gates[2];
    gate3.config(
        GatewaysConfig::Gate::GTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}}
    );

    auto& gate4 = proc2.gatewaysConfig.gates[3];
    gate4.config(
        GatewaysConfig::Gate::NEQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    auto& gate5 = proc2.gatewaysConfig.gates[4];
    gate5.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{1, {8}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    proc2.insert_gateway_entry({true, true}, {true}, {true});
    proc2.insert_gateway_entry({true, true, true, true, true}, {false, true ,true, true, true}, {false, true});

    // todo: add other entries

    proc2.matchTableConfig.match_table_num = 3;
    proc2.matchTableConfig.matchTables[0].default_action_id = 5;
    proc2.matchTableConfig.matchTables[1].default_action_id = 6;
    proc2.matchTableConfig.matchTables[2].default_action_id = 7;

    proc2.commit();

    auto& action = actionConfigs[3].actions[5];
    action.action_id = 5;
    action.vliw_enabler = {false};
    action.config_alu(163, ALUnit::OP::SET,
        // operand1
        ALUnit::Parameter::CONST,(uint32_t) 1,
        // operand2
        ALUnit::Parameter::CONST,(uint32_t) 0
    );
    action.config_alu(164, ALUnit::OP::SET,
        ALUnit::Parameter::CONST,(uint32_t) 1,
        ALUnit::Parameter::CONST,(uint32_t) 0
    );

    auto& action1 = actionConfigs[3].actions[6];
    action1.action_id = 6;
    action1.vliw_enabler = {false};
    action1.config_alu(162, ALUnit::OP::SET,
        ALUnit::Parameter::CONST,(uint32_t) 1,
        ALUnit::Parameter::CONST,(uint32_t) 0
    );

    auto& action2 = actionConfigs[3].actions[7];
    action2.action_id = 7;
    action2.vliw_enabler = {false};
    action2.config_alu(163, ALUnit::SET,
        ALUnit::Parameter::CONST, (uint32_t) 1,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );


    proc_types[3] = ProcType::WRITE;
    state_idx_in_phv[3] = {162, 163, 164};
    auto& saved_state_2 = state_saved_idxs[3];
    saved_state_2.config(3, {162, 163, 164}, {1, 1, 1});
    // processor_3 finished

    backward_cycle_num = 80;
    read_proc_ids = {0, 0, 0, 0, 1};
    write_proc_ids = {3, 4, 0, 0, 0};
    tags = {1, 2, 0, 1, 2};

//    proc_types[4] = ProcType::WRITE;
//    proc_types[1] = ProcType::READ;
    cycles_per_hb = 1;
}

void top_heavy_hitter_config1(){
    flow_id_in_phv = {166, 167};
    gateway_guider[32] = gateway_guider[33] = gateway_guider[34] = gateway_guider[35] = gateway_guider[36] = 1;

    ProcessorConfig proc0 = ProcessorConfig(0);
    proc0.push_back_get_key_use_container(0, 8, 0);
    proc0.push_back_get_key_use_container(64, 16, 1, 2);
    proc0.push_back_get_key_use_container(65, 16, 3, 4);
    proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
    proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

    proc0.insert_gateway_entry({false}, {false}, {true, true, true});


    proc0.matchTableConfig.match_table_num = 3;
    auto& config_0 = proc0.matchTableConfig.matchTables[0];
    config_0.config_match_table(
        1, {166, 167},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{0},{2},{4},{6}}},
        {{{1},{3},{5},{7}}}
    );

    auto&  config_1 = proc0.matchTableConfig.matchTables[1];
    config_1.config_match_table(
        1, {168, 169},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{8},{10},{12},{14}}},
        {{{9},{11},{13},{15}}}
    );

    auto& config_2 = proc0.matchTableConfig.matchTables[2];
    config_2.config_match_table(
        1, {170, 171},
        1, 1, 1,
        13,
        {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{16}, {18}, {20}, {22}}},
        {{{17}, {19}, {21}, {23}}}
    );


    // config_2.type = 1;
    // config_2.depth = 1;
    // config_2.key_width = 1;
    // config_2.value_width = 1;
    // config_2.hash_in_phv = {170, 171};
    // config_2.match_field_byte_len = 13;
    // config_2.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
    // config_2.number_of_hash_ways = 4;
    // config_2.hash_bit_sum = 40;
    // config_2.hash_bit_per_way = {10, 10, 10, 10};
    // config_2.srams_per_hash_way = {2, 2, 2, 2};
    // config_2.key_sram_index_per_hash_way[0] = {16};
    // config_2.key_sram_index_per_hash_way[1] = {18};
    // config_2.key_sram_index_per_hash_way[2] = {20};
    // config_2.key_sram_index_per_hash_way[3] = {22};
    // config_2.value_sram_index_per_hash_way[0] = {17};
    // config_2.value_sram_index_per_hash_way[1] = {19};
    // config_2.value_sram_index_per_hash_way[2] = {21};
    // config_2.value_sram_index_per_hash_way[3] = {23};


    // config_2.type = 1;
    // config_2.depth = 1;
    // config_2.key_width = 1;
    // config_2.value_width = 1;
    // config_2.hash_in_phv = {170, 171};
    // config_2.match_field_byte_len = 13;
    // config_2.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
    // config_2.number_of_hash_ways = 4;
    // config_2.hash_bit_sum = 40;
    // config_2.hash_bit_per_way = {10, 10, 10, 10};
    // config_2.srams_per_hash_way = {2, 2, 2, 2};
    // config_2.key_sram_index_per_hash_way[0] = {16};
    // config_2.key_sram_index_per_hash_way[1] = {18};
    // config_2.key_sram_index_per_hash_way[2] = {20};
    // config_2.key_sram_index_per_hash_way[3] = {22};
    // config_2.value_sram_index_per_hash_way[0] = {17};
    // config_2.value_sram_index_per_hash_way[1] = {19};
    // config_2.value_sram_index_per_hash_way[2] = {21};
    // config_2.value_sram_index_per_hash_way[3] = {23};

    array<bool, 224> vliw_enabler = {0};
    proc0.commit();

    auto& salu = SALUs[0][0];
    salu.config(
        224, SALUnit::IfElseRAW,
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int) 0, 3,
        SALUnit::Parameter::Type::CONST, (int)0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 1, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 162,
        SALUnit::ReturnValueFrom::Type::REG, (int) 0, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (int) 0, 0
    );


    auto& salu1 = SALUs[0][1];
    salu1.config(
        225, SALUnit::IfElseRAW,
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int) 1, 3,
        SALUnit::Parameter::Type::CONST, (int)0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 1, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 163,
        SALUnit::ReturnValueFrom::Type::REG, (int) 1, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (int) 0, 0
    );

    auto& salu2 = SALUs[0][2];
    salu2.config(
        226, SALUnit::IfElseRAW,
        SALUnit::Parameter::Type::REG,
        SALUnit::Parameter::IfType::COMPARE_EQ,
        (int) 2, 3,
        SALUnit::Parameter::Type::CONST, (int)0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 1, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 164,
        SALUnit::ReturnValueFrom::Type::REG, (int) 2, 3,
        SALUnit::ReturnValueFrom::Type::CONST, (uint32_t) 0, 0
    );


    num_of_stateful_tables[0] = 3;
    stateful_table_ids[0] = {0, 1, 2};
    proc_types[0] = ProcType::READ;
    state_idx_in_phv[0] = {162, 163, 164};

    auto& phv_id_to_save_hash_value_0 = phv_id_to_save_hash_value[0];
    phv_id_to_save_hash_value_0[0] = {166, 167};
    phv_id_to_save_hash_value_0[1] = {168, 169};
    phv_id_to_save_hash_value_0[2] = {170, 171};

    auto& salu_id_0 = salu_id[0];
    salu_id_0 = {224, 225, 226};

    auto& saved_state_0 = state_saved_idxs[0];
    saved_state_0.config(3, {162, 163, 164}, {1, 1, 1});
    // processor1 finished

    ProcessorConfig proc1 = ProcessorConfig(1);

    proc1.matchTableConfig.match_table_num = 2;
    auto& match_table_1 = proc1.matchTableConfig.matchTables[0];
    match_table_1.type = 1;
    auto& match_table_2 = proc1.matchTableConfig.matchTables[1];
    match_table_2.type = 1;
    proc1.commit();

    num_of_stateful_tables[1] = 2;
    stateful_table_ids[1] = {0, 1};
    auto& phv_id_to_save_hash_value_1 = phv_id_to_save_hash_value[1];
    phv_id_to_save_hash_value_1[0] = {172, 173};
    phv_id_to_save_hash_value_1[1] = {172, 173};

    auto& salu_id_1 = salu_id[1];
    salu_id_1 = {224, 225};
    // if phv_162 >= phv_163, phv_165 = phv_162; else: phv_165 = phv_163
    auto& salu3 = SALUs[1][0];
    salu3.config(
        224, SALUnit::OP::IfElseRAW,
        SALUnit::Parameter::Type::HEADER,
        SALUnit::Parameter::IfType::GTE,
        (int) 162, 0,
        SALUnit::Parameter::Type::HEADER, (int) 163, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 165,
        SALUnit::ReturnValueFrom::Type::LEFT, 0, 0,
        SALUnit::ReturnValueFrom::Type::OP1, 0, 0
    );

    // if phv_162 >= phv_163, phv_1 = 0; else: phv_1 = 1;
    auto& salu4 = SALUs[1][1];
    salu4.config(
        225, SALUnit::OP::IfElseRAW,
        SALUnit::Parameter::Type::HEADER,
        SALUnit::Parameter::IfType::GTE,
        (int) 162, 0,
        SALUnit::Parameter::Type::HEADER, (int) 163, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::CONST, (uint32_t) 0, 0,
        SALUnit::Parameter::Type::HEADER, (int) 1,
        SALUnit::ReturnValueFrom::CONST, (uint32_t) 0, 0,
        SALUnit::ReturnValueFrom::CONST, (uint32_t) 1, 0
    );
    // processor_2 finished, using salu instead of normal alu

    ProcessorConfig proc2 = ProcessorConfig(2);
    proc2.push_back_get_key_use_container(164, 32, 0, 1, 2, 3);
    proc2.push_back_get_key_use_container(165, 32, 4, 5, 6, 7);
    proc2.push_back_get_key_use_container(1, 8, 8);

    auto& gate1 = proc2.gatewaysConfig.gates[0];
    gate1.config(
        GatewaysConfig::Gate::OP::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}}
    );

    auto& gate2 = proc2.gatewaysConfig.gates[1];
    gate2.config(
        GatewaysConfig::Gate::OP::NEQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    auto& gate3 = proc2.gatewaysConfig.gates[2];
    gate3.config(
        GatewaysConfig::Gate::GTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {0, 1, 2, 3}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}}
    );

    auto& gate4 = proc2.gatewaysConfig.gates[3];
    gate4.config(
        GatewaysConfig::Gate::NEQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {4, 5, 6, 7}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    auto& gate5 = proc2.gatewaysConfig.gates[4];
    gate5.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{1, {8}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 0
    );

    proc2.insert_gateway_entry({true, true}, {true}, {true});
    proc2.insert_gateway_entry({true, true, true, true, true}, {false, true ,true, true, true}, {false, true});

    // todo: add other entries

    proc2.matchTableConfig.match_table_num = 3;
    proc2.matchTableConfig.matchTables[0].default_action_id = 5;
    proc2.matchTableConfig.matchTables[1].default_action_id = 6;
    proc2.matchTableConfig.matchTables[2].default_action_id = 7;

    proc2.commit();

    auto& action = actionConfigs[2].actions[5];
    action.action_id = 5;
    action.vliw_enabler = {false};
    action.config_alu(163, ALUnit::OP::SET,
        ALUnit::Parameter::CONST,(uint32_t) 1,
        ALUnit::Parameter::CONST,(uint32_t) 0
    );
    action.config_alu(164, ALUnit::OP::SET,
        ALUnit::Parameter::CONST,(uint32_t) 1,
        ALUnit::Parameter::CONST,(uint32_t) 0
    );

    auto& action1 = actionConfigs[2].actions[6];
    action1.action_id = 6;
    action1.vliw_enabler = {false};
    action1.config_alu(162, ALUnit::OP::SET,
        ALUnit::Parameter::CONST,(uint32_t) 1,
        ALUnit::Parameter::CONST,(uint32_t) 0
    );

    auto& action2 = actionConfigs[2].actions[7];
    action2.action_id = 7;
    action2.vliw_enabler = {false};
    action2.config_alu(163, ALUnit::SET,
        ALUnit::Parameter::CONST, (uint32_t) 1,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );

    proc_types[2] = ProcType::WRITE;
    state_idx_in_phv[2] = {162, 163, 164};
    auto& saved_state_2 = state_saved_idxs[2];
    saved_state_2.config(3, {162, 163, 164}, {1, 1, 1});
    // processor_3 finished

    backward_cycle_num = 50;
    read_proc_ids = {0};
    write_proc_ids = {2};
    cycles_per_hb = 1;
}

void nat(){
    flow_id_in_phv = {171, 172};


    ProcessorConfig proc0 = ProcessorConfig(0);

    proc0.push_back_get_key_use_container(0, 8, 0);
    proc0.push_back_get_key_use_container(64, 16, 1, 2);
    proc0.push_back_get_key_use_container(65, 16, 3, 4);
    proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
    proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

    proc0.insert_gateway_entry({false}, {false}, {true});

    proc0.matchTableConfig.match_table_num = 1;
    auto& config_0 = proc0.matchTableConfig.matchTables[0];
    config_0.config_match_table(
        1, {171, 172},
        1, 1, 1,
        13, {0,1,2,3,4,5,6,7,8,9,10,11,12},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{0},{2},{4},{6}}},
        {{{1},{3},{5},{7}}}
    );

    proc0.commit();

    auto& salu00 = SALUs[0][0];
    salu00.salu_id = 224;
    salu00.op = SALUnit::IfElseRAW;
    salu00.left_value.config(SALUnit::Parameter::Type::HEADER,
        SALUnit::Parameter::IfType::COMPARE_EQ, (int) 3, 0);
    salu00.operand2.config(SALUnit::Parameter::Type::CONST, (uint32_t) 1);
    salu00.operand3.config(SALUnit::Parameter::Type::CONST, (uint32_t) 0);

    auto& salu01 = SALUs[0][1];
    salu01.salu_id = 225;
    salu01.op = SALUnit::PRAW;
    salu01.left_value.config(SALUnit::Parameter::HEADER,
        SALUnit::Parameter::COMPARE_EQ, (int) 162, 0);
    salu01.operand2.config(SALUnit::Parameter::REG, (uint32_t) 0, 3);

    num_of_stateful_tables[0] = 1;
    stateful_table_ids[0] = {0};
    proc_types[0] = READ;
    state_idx_in_phv[0] = {162};

    phv_id_to_save_hash_value[0][0] = {171, 172};
    salu_id[0] = {224, 225};

    auto& ss_0 = state_saved_idxs[0];
    ss_0.config(1, {1}, {162});
    // processor 1 finished

    ProcessorConfig proc1 = ProcessorConfig(1);
    proc1.push_back_get_key_use_container(3, 8, 0);
    proc1.push_back_get_key_use_container(161, 32, 1, 2, 3, 4);

    proc1.insert_gateway_entry({false}, {false}, {true, true});

    proc1.matchTableConfig.match_table_num = 2;
    auto& config_1 = proc1.matchTableConfig.matchTables[0];
    config_1.config_match_table(
        1, {173, 174},
        1, 1, 1,
        4, {1, 2, 3, 4},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{0},{2},{4},{6}}},
        {{{1},{3},{5},{7}}}
    );

    auto& config_2 = proc1.matchTableConfig.matchTables[1];
    config_2.config_match_table(
        1, {175, 176},
        1, 1, 1,
        4, {1, 2, 3, 4},
        4, 40,
        {10, 10, 10, 10},
        {2, 2, 2, 2},
        {{{8}, {10}, {12}, {14}}},
        {{{9}, {11}, {13}, {15}}}
    );

    proc1.commit();

    auto& salu10 = SALUs[1][0];
    salu10.salu_id = 224;
    salu10.op = SALUnit::READ;
    salu10.left_value.config(SALUnit::Parameter::HEADER, (int) 163);
    salu10.operand1.config(SALUnit::Parameter::REG, (int) 0, 0);

    auto& salu11 = SALUs[1][1];
    salu11.salu_id = 225;
    salu11.op = SALUnit::READ;
    salu11.left_value.config(SALUnit::Parameter::HEADER, (int) 164);
    salu11.operand1.config(SALUnit::Parameter::REG, (int) 0, 1);

    auto& salu12 = SALUs[1][2];
    salu12.salu_id = 226;
    salu12.op = SALUnit::READ;
    salu12.left_value.config(SALUnit::Parameter::HEADER, (int) 165);
    salu12.operand1.config(SALUnit::Parameter::REG, (int) 0, 2);

    auto& salu13 = SALUs[1][3];
    salu13.salu_id = 227;
    salu13.op = SALUnit::READ;
    salu13.left_value.config(SALUnit::Parameter::HEADER, (int) 166);
    salu13.operand1.config(SALUnit::Parameter::REG, (int) 0, 3);

    auto& salu14 = SALUs[1][4];
    salu14.salu_id = 228;
    salu14.op = SALUnit::READ;
    salu14.left_value.config(SALUnit::Parameter::HEADER, (int) 167);
    salu14.operand1.config(SALUnit::Parameter::REG, (int) 1, 0);

    auto& salu15 = SALUs[1][5];
    salu15.salu_id = 229;
    salu15.op = SALUnit::READ;
    salu15.left_value.config(SALUnit::Parameter::HEADER, (int) 168);
    salu15.operand1.config(SALUnit::Parameter::REG, (int) 1, 1);

    auto& salu16 = SALUs[1][6];
    salu16.salu_id = 230;
    salu16.op = SALUnit::READ;
    salu16.left_value.config(SALUnit::Parameter::HEADER, (int) 169);
    salu16.operand1.config(SALUnit::Parameter::REG, (int) 1, 2);

    auto& salu17 = SALUs[1][7];
    salu17.salu_id = 231;
    salu17.op = SALUnit::READ;
    salu17.left_value.config(SALUnit::Parameter::HEADER, (int) 170);
    salu17.operand1.config(SALUnit::Parameter::REG, (int) 1, 3);

    num_of_stateful_tables[1] = 2;
    stateful_table_ids[1] = {0, 1};
    proc_types[1] = ProcType::READ;
    state_idx_in_phv[1] = {163, 164, 165, 166, 167, 168, 169, 170};

    auto& phv_id_to_save_hash_value_1 = phv_id_to_save_hash_value[1];
    phv_id_to_save_hash_value_1[0] = {173, 174};
    phv_id_to_save_hash_value_1[1] = {175, 176};

    auto& salu_id_1 = salu_id[1];
    salu_id_1 = {224, 225, 226, 227, 228, 229, 230, 231};

    auto& saved_state_1 = state_saved_idxs[1];
    saved_state_1.config(2, {163, 164, 165, 166, 167, 168, 169, 170} ,{4, 4});
    // processor 2 finished

    ProcessorConfig proc2 = ProcessorConfig(2);
    for(int i = 0; i < 8; i++){
        proc2.push_back_get_key_use_container(163+i, 32, 4*i, 4*i+1, 4*i+2, 4*i+3);
    }
    proc2.push_back_get_key_use_container(162, 32, 32, 33, 34, 35);
    proc2.push_back_get_key_use_container(3, 8, 36);

    auto& gate_21 = proc2.gatewaysConfig.gates[0];
    gate_21.config(
        GatewaysConfig::Gate::OP::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{1, {36}},
        GatewaysConfig::Gate::Parameter::CONST,
        (uint32_t) 1
    );

    auto& gate_22 = proc2.gatewaysConfig.gates[1];
    gate_22.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {16, 17, 18, 19}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {20, 21, 22, 23}}
    );

    auto& gate_23 = proc2.gatewaysConfig.gates[2];
    gate_23.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {16, 17, 18, 19}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {24, 25, 26, 27}}
    );

    auto& gate_24 = proc2.gatewaysConfig.gates[3];
    gate_24.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {16, 17, 18, 19}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {28, 29, 30, 31}}
    );

    auto& gate_25 = proc2.gatewaysConfig.gates[4];
    gate_25.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {20, 21, 22, 23}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {24, 25, 26, 27}}
    );

    auto& gate_26 = proc2.gatewaysConfig.gates[5];
    gate_26.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {20, 21, 22, 23}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {28, 29, 30, 31}}
    );

    auto& gate_27 = proc2.gatewaysConfig.gates[6];
    gate_27.config(
        GatewaysConfig::Gate::LTE,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {24, 25, 26, 27}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {28, 29, 30, 31}}
    );

    auto& gate_28 = proc2.gatewaysConfig.gates[7];
    gate_28.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {32, 33, 34, 35}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {16, 17, 18, 19}}
    );

    auto& gate_29 = proc2.gatewaysConfig.gates[8];
    gate_29.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {32, 33, 34, 35}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {20, 21, 22, 23}}
    );

    auto& gate_210 = proc2.gatewaysConfig.gates[9];
    gate_210.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {32, 33, 34, 35}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {24, 25, 26, 27}}
    );

    auto& gate_211 = proc2.gatewaysConfig.gates[10];
    gate_211.config(
        GatewaysConfig::Gate::EQ,
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {32, 33, 34, 35}},
        GatewaysConfig::Gate::Parameter::HEADER,
        GatewaysConfig::Gate::Parameter::Content::Operand_match_field_byte{4, {28, 29, 30, 31}}
    );

    proc2.set_gateway_enable({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

    proc2.insert_gateway_entry({true, true, true, true}, {false, true, true, true}, {true});
    proc2.insert_gateway_entry({true, true, false, false, true}, {false, false, false, false, true, true}, {false, true});
    proc2.insert_gateway_entry({true, false, true, false, true, false, true}, {false, false, false, false, false, false, true}, {false, false, true});
    proc2.insert_gateway_entry({true, false, false, false, true, false, true}, {}, {false, false, false, true});

    proc2.insert_gateway_entry({true, false, false, false, false, false, false, true}, {true, false, false, false, false, false, false, true}, {false, false, false, false, true});
    proc2.insert_gateway_entry({true, false, false, false, false, false, false, false, true}, {true, false, false, false, false, false, false, false, true}, {false, false, false, false, false, true});
    proc2.insert_gateway_entry({true, false, false, false, false, false, false, false, false, true}, {true, false, false, false, false, false, false, false, false, true}, {false, false, false, false, false, false, true});
    proc2.insert_gateway_entry({true, false, false, false, false, false, false, false, false, false, true}, {true, false, false, false, false, false, false, false, false, false, true}, {false, false, false, false, false, false, false, true});

    proc2.matchTableConfig.match_table_num = 8;
    for(int i = 0; i < 8; i++){
        proc2.matchTableConfig.matchTables[i].default_action_id = i;
    }

    proc2.commit();

    auto& action20 = actionConfigs[2].actions[0];
    action20.action_id = 0;
    action20.vliw_enabler = {false};
    action20.config_alu(162, ALUnit::OP::SET,
        ALUnit::Parameter::HEADER, (int) 163,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );
    action20.config_alu(167, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 167
    );

    auto& action21 = actionConfigs[2].actions[1];
    action21.action_id = 1;
    action21.vliw_enabler = {false};
    action21.config_alu(162, ALUnit::SET,
        ALUnit::Parameter::HEADER, (int) 164,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );
    action21.config_alu(168, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 168
    );

    auto& action22 = actionConfigs[2].actions[2];
    action22.action_id = 2;
    action22.vliw_enabler = {false};
    action22.config_alu(162, ALUnit::SET,
        ALUnit::Parameter::HEADER, (int) 165,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );
    action22.config_alu(169, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 169
    );

    auto& action23 = actionConfigs[2].actions[3];
    action23.action_id = 3;
    action23.vliw_enabler = {false};
    action23.config_alu(162, ALUnit::SET,
        ALUnit::Parameter::HEADER, (int) 166,
        ALUnit::Parameter::CONST, (uint32_t) 0
    );
    action23.config_alu(170, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 170
    );

    auto& action24 = actionConfigs[2].actions[4];
    action24.action_id = 4;
    action24.vliw_enabler = {false};
    action24.config_alu(167, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 167
    );

    auto& action25 = actionConfigs[2].actions[5];
    action25.action_id = 5;
    action25.vliw_enabler = {false};
    action25.config_alu(168, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 168
    );

    auto& action26 = actionConfigs[2].actions[6];
    action26.action_id = 6;
    action26.vliw_enabler = {false};
    action26.config_alu(169, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 169
    );

    auto& action27 = actionConfigs[2].actions[7];
    action27.action_id = 7;
    action27.vliw_enabler = {false};
    action27.config_alu(170, ALUnit::PLUS,
        ALUnit::Parameter::HEADER, (int) 2,
        ALUnit::Parameter::HEADER, (int) 170
    );

    proc_types[2] = WRITE;
    state_idx_in_phv[2] = {162};
    auto& saved_state_2 = state_saved_idxs[2];
    saved_state_2.config(1, {162}, {1});
    // processor 3 finished

    proc_types[3] = WRITE;
    state_idx_in_phv[3] = {163, 164, 165, 166, 167, 168, 169, 170};
    auto& saved_state_3 = state_saved_idxs[3];
    saved_state_3.config(2, {163, 164, 165, 166, 167, 168, 169, 170}, {4, 4});
    // processor 4 finished

    read_proc_ids = {0, 0, 0, 1};
    write_proc_ids = {2, 3, 0, 0};
    backward_cycle_num = 75;
    cycles_per_hb = 2;
}

void for_testing(){
    flow_id_in_phv = {171, 172};

    ProcessorConfig proc0 = ProcessorConfig(0);
    proc0.push_back_get_key_use_container(0, 8, 0);
    proc0.push_back_get_key_use_container(64, 16, 1, 2);
    proc0.push_back_get_key_use_container(65, 16, 3, 4);
    proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
    proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

    proc0.insert_gateway_entry({false}, {false}, {true});

    proc0.matchTableConfig.match_table_num = 1;
    auto& config_0 = proc0.matchTableConfig.matchTables[0];
    config_0.config_match_table(
            1, {171, 172},
            1, 1, 1,
            13, {0,1,2,3,4,5,6,7,8,9,10,11,12},
            4, 40,
            {10, 10, 10, 10},
            {2, 2, 2, 2},
            {{{0},{2},{4},{6}}},
            {{{1},{3},{5},{7}}}
    );

    proc_types[0] = READ;
    proc0.commit();

    num_of_stateful_tables[0] = 1;
    stateful_table_ids[0] = {0};

    phv_id_to_save_hash_value[0][0] = {171, 172};

    proc_types[test_write_proc_id] = WRITE;
    read_proc_ids = {0};
    write_proc_ids = {test_write_proc_id};
    backward_cycle_num = 30 * test_write_proc_id;
    cycles_per_hb = 1;

}

struct Switch
{

    Parser parser;
    PipeLine* pipeline;
    PipeLine* next;
    vector<unique_ptr<Logic>> logics;

    Switch()
    {
        pipeline = new PipeLine();
        pipeline->set_id();
        for (int i = 0; i < PROCESSOR_NUM; i++)
        {
            // done Logic 需要一个虚析构函数，但现在不算什么大问题还
            logics.push_back(make_unique<GetKey>(i));
            logics.push_back(make_unique<Gateway>(i));
            logics.push_back(make_unique<GetHash>(i));
            logics.push_back(make_unique<GetAddress>(i));
            logics.push_back(make_unique<Matches>(i));
            logics.push_back(make_unique<Compare>(i));
            logics.push_back(make_unique<GetAction>(i));
            logics.push_back(make_unique<ExecuteAction>(i));
            logics.push_back(make_unique<VerifyStateChange>(i));
            logics.push_back(make_unique<PIR_asyn>(i));
            logics.push_back(make_unique<PIR>(i));
            logics.push_back(make_unique<PIW>(i));
            logics.push_back(make_unique<PO>(i));
            logics.push_back(make_unique<RI>(i));
            logics.push_back(make_unique<RO>(i));
        }
    }

    void GetInput(int interface, const Packet &packet, PipeLine* pipeline_, int arrive_id)
    {
        if (interface != 0)
        {
            PHV phv = parser.parse(packet);
            // todo: change between
            // todo: change between
            phv[ID_IN_PHV] = arrive_id;
            pipeline_->processors[0].getKeys.enable1 = true;
            pipeline_->processors[0].getKeys.phv = phv;
            pipeline_->processors[0].getKeys.gateway_guider = gateway_guider;
            pipeline_->processors[0].getKeys.match_table_guider = {};
        }
        else
        {
            pipeline_->processors[0].getKeys.enable1 = false;
            pipeline_->processors[0].getKeys.phv = {};
            pipeline_->processors[0].getKeys.gateway_guider = {};
            pipeline_->processors[0].getKeys.match_table_guider = {};
        }
    }

    void Execute(int interface, const Packet &packet, int arrive_id)
    {
        next = new PipeLine();
        next->set_id();
        GetInput(interface, packet, next, arrive_id);
        for(int i = 0; i < PROCESSOR_NUM; i++){
            Update();
            next->proc_states[i] = pipeline->proc_states[i];
        }
        for (auto &logic : logics)
        {
            logic->execute(pipeline, next);
        }
        GetOutput();
        delete pipeline;
        pipeline = next;
    }

    void GetOutput()
    {
        PIWRegister &output = pipeline->processors[PROCESSOR_NUM-1].piw[0];
        if (output.enable1)
        {
            output.phv;
        }
    }

    std::pair<u64, int> get_output_arrive_id() const{
        PIWRegister &output = pipeline->processors[PROCESSOR_NUM-1].piw[0];
        if(output.enable1){
            u64 flow_id = u32_to_u64(output.phv[flow_id_in_phv[0]], output.phv[flow_id_in_phv[1]]);
            return {flow_id, output.phv[ID_IN_PHV]};
        }
        else{
            return {-1, -1};
        }
    }

    void Log(std::array<ofstream*, PROCESSOR_NUM> outputs, std::array<bool, PROCESSOR_NUM> enable){
        for(int i = 0; i < PROCESSOR_NUM; i++){
            if(enable[i]){
                pipeline->proc_states[i].log(*outputs[i]);
            }
        }
    };

    void Update(){
        for(int i = 0; i < PROCESSOR_NUM; i++){
                pipeline->proc_states[i].update();
        }
    }

    bool VerifyQueueOversize(){
        for(int i = 0; i < PROCESSOR_NUM; i++){
            auto proc_state = pipeline->proc_states[i];
            if(proc_state.rp2p_max > TEST_MAX_RP2P || proc_state.max_dirty_cam > TEST_MAX_DIRTY_CAM ||
            proc_state.m_wait_queue > TEST_MAX_WAIT_QUEUE || proc_state.m_schedule_queue > TEST_MAX_SCHEDULE_QUEUE ||
            proc_state.r2p_stash_max > TEST_MAX_R2P || proc_state.p2r_max > TEST_MAX_P2R || proc_state.r2r_max > TEST_MAX_R2R){
                return true;
            }
        }
        return false;
    }

    void Config()
    {
        for_testing();
//        nat();
    }

    static void insertKeyValuePairToMatchTable(int processor_id, MatchTableConfig::MatchTable match_table, PHV phv, std::array<b128, 8> value){
        // get key
        GetKeyConfig getKeyConfig = getKeyConfigs[processor_id];
        std::array<u32, 32> key{};
        for (int i = 0; i < getKeyConfig.used_container_num; i++)
        {
            GetKeyConfig::UsedContainer2MatchFieldByte it = getKeyConfig.used_container_2_match_field_byte[i];
            int id = it.used_container_id;
            if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U8)
            {
                key[it.match_field_byte_ids[0]] = phv[id];
            }
            else if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U16)
            {
                key[it.match_field_byte_ids[0]] = phv[id] << 16 >> 24;
                key[it.match_field_byte_ids[1]] = phv[id] << 24 >> 24;
            }
            else
            {
                key[it.match_field_byte_ids[0]] = phv[id] >> 24;
                key[it.match_field_byte_ids[1]] = phv[id] << 8 >> 24;
                key[it.match_field_byte_ids[2]] = phv[id] << 16 >> 24;
                key[it.match_field_byte_ids[3]] = phv[id] << 24 >> 24;
            }
        }

        // hash
        std::array<u32, 32> match_table_key{};
        for (int j = 0; j < match_table.match_field_byte_len; j++)
        {
            match_table_key[j] = key[match_table.match_field_byte_ids[j]];
        }
        std::array<u32, 4> hash_values{};
        auto hash_unit = ArrayHash();
        auto hash_value = hash_unit(match_table_key, match_table.hash_bit_sum);

        for(int i = 0; i < match_table.number_of_hash_ways; i++){
            hash_values[i] = hash_value & ((1 << match_table.hash_bit_per_way[i]) - 1);
            hash_value = hash_value >> match_table.hash_bit_per_way[i];
        }

        // write
        for (int j = 0; j < match_table.number_of_hash_ways; j++)
        {
            // translate from hash to addr
            u32 start_key_index = (hash_values[j] >> 10) * match_table.key_width;
            u32 start_value_index = (hash_values[j] >> 10) * match_table.value_width;
            auto on_chip_addr = (hash_values[j] << 22 >> 22);
            std::array<int, 8> key_sram_columns{};
            std::array<int, 8> value_sram_columns{};
            std::array<b128, 8> obtained_key{};

            for (int k = 0; k < match_table.key_width; k++)
            {
                // in fact, key_width & value_width here must be 1
                key_sram_columns[k] = match_table.key_sram_index_per_hash_way[j][start_key_index + k];
                obtained_key[k] = SRAMs[processor_id][key_sram_columns[k]].get(on_chip_addr);
            }
            for (int k = 0; k < match_table.value_width; k++)
            {
                value_sram_columns[k] = match_table.value_sram_index_per_hash_way[j][start_value_index + k];
            }

            // compare key & flow_id (here use flow_id as identifier of a match key)

            bool vacant_flag = true;
            for(int k = 0; k < match_table.key_width; k++){
                if(obtained_key[k][0] != 0 || obtained_key[k][1] != 0 || obtained_key[k][2] != 0 || obtained_key[k][3] != 0){
                    vacant_flag = false;
                    break;
                }
            }
            if(vacant_flag){
                // vacant entry
                // write value to proper position
                if(match_table.type == 1){
                    SRAMs[processor_id][key_sram_columns[0]].set(int(on_chip_addr), hash_values);
                }
                else if(match_table.type == 0){
                    for(int k = 0; k < match_table.key_width; k++){
                        SRAMs[processor_id][key_sram_columns[k]].set(int(on_chip_addr), {match_table_key[4*k], match_table_key[4*k+1], match_table_key[4*k+2], match_table_key[4*k+3]});
                    }
                }
                for(int k = 0; k < match_table.value_width; k++){
                    SRAMs[processor_id][value_sram_columns[k]].set(int(on_chip_addr), value[k]);
                }
                break;
            }
        }
    }

    void dataplaneConfig(){

    }

};

#endif // RPISA_SW_SWITCH_H
