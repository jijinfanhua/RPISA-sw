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
using byte = unsigned char;

struct ProcessorConfig
{

    GetKeyConfig getKeyConfig{};
    GatewaysConfig gatewaysConfig;
    MatchTableConfig matchTableConfig{};
    ActionConfig actionConfig{};

    int processor_id;

    explicit ProcessorConfig(int id) : processor_id(id)
    {
        getKeyConfig.processor_id = id;
        getKeyConfig.used_container_num = 0;
        gatewaysConfig.processor_id = id;
        matchTableConfig.processor_id = id;
        matchTableConfig.match_table_num = 0;
        actionConfig.processor_id = id;
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
            sum += (1 << (15 - i));
        }
        return sum;
    }

public:
    void insert_gateway_res_2_match_table(
        const array<bool, GATEWAY_NUM> &key,
        const array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> &value)
    {
        gatewaysConfig.gateway_res_2_match_tables[bool_array_2_u32(key)] = value;
    }

    void insert_gateway_res_2_gates(
        const array<bool, GATEWAY_NUM> &key,
        const array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> &value)
    {
        gatewaysConfig.gateway_res_2_gates[bool_array_2_u32(key)] = value;
    }

    GatewaysConfig::Gate::Parameter param(int value)
    {
        GatewaysConfig::Gate::Parameter parameter;
        parameter.type = GatewaysConfig::Gate::Parameter::Type::CONST;
        parameter.content.value = value;
        return parameter;
    }

    GatewaysConfig::Gate::Parameter param(int len, int id1, int id2 = -1, int id3 = -1, int id4 = -1)
    {
        GatewaysConfig::Gate::Parameter parameter;
        parameter.type = GatewaysConfig::Gate::Parameter::Type::HEADER;
        if (len >= 1)
            parameter.content.operand_match_field_byte.match_field_byte_ids[0] = id1;
        if (len >= 2)
            parameter.content.operand_match_field_byte.match_field_byte_ids[1] = id2;
        if (len >= 3)
            parameter.content.operand_match_field_byte.match_field_byte_ids[2] = id3;
        if (len >= 4)
            parameter.content.operand_match_field_byte.match_field_byte_ids[3] = id4;
        return parameter;
    }

    void set_gateway_gate(
        int id,
        GatewaysConfig::Gate::Parameter operand1,
        GatewaysConfig::Gate::OP op,
        GatewaysConfig::Gate::Parameter operand2)
    {
        gatewaysConfig.gates[id].op = op;
        gatewaysConfig.gates[id].operand1 = operand1;
        gatewaysConfig.gates[id].operand2 = operand2;
    }

    void push_back_match_table_config(
        int depth, int key_width, int value_width)
    {
        int k = matchTableConfig.match_table_num;
        if (k >= 4)
        {
            throw "match table config already full";
        }
        auto &config = matchTableConfig.matchTables[k];
        config.depth = depth;
        config.key_width = key_width;
        config.value_width = value_width;
        config.match_field_byte_len = 0;
        config.number_of_hash_ways = 0;
        config.hash_bit_sum = 0;
        matchTableConfig.match_table_num += 1;
    }

    void set_back_match_table_match_field_byte(
        const vector<int> &ids)
    {
        auto &config = matchTableConfig.matchTables[matchTableConfig.match_table_num - 1];
        for (int id : ids)
        {
            config.match_field_byte_ids[config.match_field_byte_len++] = id;
        }
    }

    void push_back_hash_way_config(
        int hash_bit, int srams,
        const array<int, 80> &key_sram_index,
        const array<int, 80> &value_sram_index)
    {
        auto &config = matchTableConfig.matchTables[matchTableConfig.match_table_num - 1];
        int k = config.number_of_hash_ways;
        config.hash_bit_per_way[k] = hash_bit;
        config.srams_per_hash_way[k] = srams;
        config.key_sram_index_per_hash_way[k] = key_sram_index;
        config.value_sram_index_per_hash_way[k] = value_sram_index;
        config.hash_bit_sum += config.hash_bit_per_way[k];
        config.number_of_hash_ways++;
    }

    void set_action_param_num(int id, int action_id, int action_data_num, const array<bool, MAX_PHV_CONTAINER_NUM> &vliw_enabler)
    {
        auto &action = actionConfig.actions[id];
        action.action_id = action_id;
        action.action_data_num = action_data_num;
        action.vliw_enabler = vliw_enabler;
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

struct Switch
{

    Parser parser;
    PipeLine* pipeline;
    PipeLine* next;
    vector<unique_ptr<Logic>> logics;
    int proc_num_config;

    Switch()
    {
        pipeline = new PipeLine();
        for (int i = 0; i < PROC_NUM; i++)
        {
            // todo: Logic 需要一个虚析构函数，但现在不算什么大问题还
            logics.push_back(make_unique<GetKey>(i));
            logics.push_back(make_unique<Gateway>(i));
            logics.push_back(make_unique<GetHash>(i));
            logics.push_back(make_unique<GetAddress>(i));
            logics.push_back(make_unique<Matches>(i));
            logics.push_back(make_unique<Compare>(i));
            logics.push_back(make_unique<GetAction>(i));
            logics.push_back(make_unique<ExecuteAction>(i));
            logics.push_back(make_unique<VerifyStateChange>(i));
            logics.push_back(make_unique<PIR>(i));
            logics.push_back(make_unique<PIW>(i));
            logics.push_back(make_unique<PO>(i));
            logics.push_back(make_unique<RI>(i));
            logics.push_back(make_unique<RO>(i));
            logics.push_back(make_unique<PIR_asyn>(i));
        }
    }

    void GetInput(int interface, const Packet &packet, PipeLine* pipeline_)
    {
        if (interface != 0)
        {
            PHV phv = parser.parse(packet);
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

    void Execute(int interface, const Packet &packet)
    {
        next = new PipeLine();
        GetInput(interface, packet, next);
        for(int i = 0; i < PROC_NUM; i++){
            next->proc_states[i] = pipeline->proc_states[i];
        }
        for (auto &logic : logics)
        {
            logic->execute(pipeline, next);
        }
        GetOutput();
        Log();
        delete pipeline;
        pipeline = next;
    }

    void GetOutput()
    {
        BaseRegister &output = pipeline->processors[proc_num_config].base;
        if (output.enable1)
        {
            // todo: 这里实际是找到payload，然后给phv装上去，现在还没有材料可以用
            output.phv;
        }
    }

    void Log(){
        for(int i = 0; i < PROC_NUM; i++){
            cout << "processor_id: " << i << endl;
            pipeline->proc_states[i].log();
        }
    };

    void Config()
    {
        flow_id_in_phv = {166, 167};
        gateway_guider[32] = gateway_guider[33] = gateway_guider[34] = gateway_guider[35] = gateway_guider[36] = 1;

        ProcessorConfig proc0 = ProcessorConfig(0);
        proc0.push_back_get_key_use_container(0, 8, 0);
        proc0.push_back_get_key_use_container(64, 16, 1, 2);
        proc0.push_back_get_key_use_container(65, 16, 3, 4);
        proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
        proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

        array<bool, GATEWAY_NUM> key = {false};
        array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> value = {0};
        value[0] = value[1] = value[2] = 1;
        proc0.insert_gateway_res_2_match_table(key, value);
        proc0.gatewaysConfig.masks.push_back(std::array<bool, 16>{false});


        proc0.matchTableConfig.match_table_num = 3;
        auto& config_0 = proc0.matchTableConfig.matchTables[0];
        config_0.type = 1;
        config_0.depth = 1;
        config_0.key_width = 1;
        config_0.value_width = 1;
        config_0.hash_in_phv = {166, 167};
        config_0.match_field_byte_len = 13;
        config_0.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config_0.number_of_hash_ways = 4; // stateful tables also using 4 way hash
        config_0.hash_bit_sum = 40;
        config_0.hash_bit_per_way = {10, 10, 10, 10};
        config_0.srams_per_hash_way = {2, 2, 2, 2};
        config_0.key_sram_index_per_hash_way[0] = {0};
        config_0.key_sram_index_per_hash_way[1] = {2};
        config_0.key_sram_index_per_hash_way[2] = {4};
        config_0.key_sram_index_per_hash_way[3] = {6};
        config_0.value_sram_index_per_hash_way[0] = {1};
        config_0.value_sram_index_per_hash_way[1] = {3};
        config_0.value_sram_index_per_hash_way[2] = {5};
        config_0.value_sram_index_per_hash_way[3] = {7};

        auto&  config_1 = proc0.matchTableConfig.matchTables[1];
        config_1.type = 1;
        config_1.depth = 1;
        config_1.key_width = 1;
        config_1.value_width = 1;
        config_1.hash_in_phv = {168, 169};
        config_1.match_field_byte_len = 13;
        config_1.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config_1.number_of_hash_ways = 4;
        config_1.hash_bit_sum = 40;
        config_1.hash_bit_per_way = {10, 10, 10, 10};
        config_1.srams_per_hash_way = {2, 2, 2, 2};
        config_1.key_sram_index_per_hash_way[0] = {8};
        config_1.key_sram_index_per_hash_way[1] = {10};
        config_1.key_sram_index_per_hash_way[2] = {12};
        config_1.key_sram_index_per_hash_way[3] = {14};
        config_1.value_sram_index_per_hash_way[0] = {9};
        config_1.value_sram_index_per_hash_way[1] = {11};
        config_1.value_sram_index_per_hash_way[2] = {13};
        config_1.value_sram_index_per_hash_way[3] = {15};

        auto& config_2 = proc0.matchTableConfig.matchTables[2];
        config_2.type = 1;
        config_2.depth = 1;
        config_2.key_width = 1;
        config_2.value_width = 1;
        config_2.hash_in_phv = {170, 171};
        config_2.match_field_byte_len = 13;
        config_2.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config_2.number_of_hash_ways = 4;
        config_2.hash_bit_sum = 40;
        config_2.hash_bit_per_way = {10, 10, 10, 10};
        config_2.srams_per_hash_way = {2, 2, 2, 2};
        config_2.key_sram_index_per_hash_way[0] = {16};
        config_2.key_sram_index_per_hash_way[1] = {18};
        config_2.key_sram_index_per_hash_way[2] = {20};
        config_2.key_sram_index_per_hash_way[3] = {22};
        config_2.value_sram_index_per_hash_way[0] = {17};
        config_2.value_sram_index_per_hash_way[1] = {19};
        config_2.value_sram_index_per_hash_way[2] = {21};
        config_2.value_sram_index_per_hash_way[3] = {23};

        array<bool, 224> vliw_enabler = {0};
        vliw_enabler[224] = 1;
        proc0.set_action_param_num(0, 0, 1, vliw_enabler);
        vliw_enabler = {0};
        vliw_enabler[225] = 1;
        proc0.set_action_param_num(1, 1, 1, vliw_enabler);
        vliw_enabler = {0};
        vliw_enabler[226] = 1;
        proc0.set_action_param_num(2, 2, 1, vliw_enabler);

        proc0.commit();

        auto& salu = SALUs[0][0];
        salu.salu_id = 224;
        salu.op = SALUnit::IfElseRAW;
        salu.left_value.type = SALUnit::Parameter::Type::REG;
        salu.left_value.if_type = SALUnit::Parameter::IfType::COMPARE_EQ;
        salu.left_value.content.table_idx = 0;
        salu.left_value.value_idx = 3;
        salu.operand2.type = SALUnit::Parameter::Type::CONST;
        salu.operand2.content.value = 1;
        salu.operand3.type = SALUnit::Parameter::Type::CONST;
        salu.operand3.content.value = 0;
        salu.return_value.type = SALUnit::Parameter::Type::HEADER;
        salu.return_value.content.phv_id = 162;
        salu.return_value_from.type = SALUnit::ReturnValueFrom::Type::REG;
        salu.return_value_from.content.table_idx = 0;
        salu.return_value_from.value_idx = 3;
        salu.return_value_from.false_type = SALUnit::ReturnValueFrom::Type::CONST;
        salu.return_value_from.false_content.value = 0;

        auto& salu1 = SALUs[0][1];
        salu1.salu_id = 225;
        salu1.op = SALUnit::IfElseRAW;
        salu1.left_value.type = SALUnit::Parameter::Type::REG;
        salu1.left_value.if_type = SALUnit::Parameter::IfType::COMPARE_EQ;
        salu1.left_value.content.table_idx = 1;
        salu1.left_value.value_idx = 3;
        salu1.operand2.type = SALUnit::Parameter::Type::CONST;
        salu1.operand2.content.value = 1;
        salu1.operand3.type = SALUnit::Parameter::Type::CONST;
        salu1.operand3.content.value = 0;
        salu1.return_value.type = SALUnit::Parameter::Type::HEADER;
        salu1.return_value.content.phv_id = 163;
        salu1.return_value_from.type = SALUnit::ReturnValueFrom::Type::REG;
        salu1.return_value_from.content.table_idx = 1;
        salu1.return_value_from.value_idx = 3;
        salu1.return_value_from.false_type = SALUnit::ReturnValueFrom::Type::CONST;
        salu1.return_value_from.false_content.value = 0;

        auto& salu2 = SALUs[0][2];
        salu2.salu_id = 226;
        salu2.op = SALUnit::IfElseRAW;
        salu2.left_value.type = SALUnit::Parameter::Type::REG;
        salu2.left_value.if_type = SALUnit::Parameter::IfType::COMPARE_EQ;
        salu2.left_value.content.table_idx = 2;
        salu2.left_value.value_idx = 3;
        salu2.operand2.type = SALUnit::Parameter::Type::CONST;
        salu2.operand2.content.value = 1;
        salu2.operand3.type = SALUnit::Parameter::Type::CONST;
        salu2.operand3.content.value = 0;
        salu2.return_value.type = SALUnit::Parameter::Type::HEADER;
        salu2.return_value.content.phv_id = 164;
        salu2.return_value_from.type = SALUnit::ReturnValueFrom::Type::REG;
        salu2.return_value_from.content.table_idx = 2;
        salu2.return_value_from.value_idx = 3;
        salu2.return_value_from.false_type = SALUnit::ReturnValueFrom::Type::CONST;
        salu2.return_value_from.false_content.value = 0;

        num_of_stateful_tables[0] = 3;
        stateful_table_ids[0] = {0, 1, 2};
        proc_types[0] = ProcType::READ;
        state_idx_in_phv[0] = {162, 163, 164};
        auto& saved_state_0 = state_saved_idxs[0];
        saved_state_0.state_num = 3;
        saved_state_0.state_lengths = {1, 1, 1};
        saved_state_0.saved_state_idx_in_phv = {162, 163, 164};

        auto& phv_id_to_save_hash_value_0 = phv_id_to_save_hash_value[0];
        phv_id_to_save_hash_value_0[0] = {166, 167};
        phv_id_to_save_hash_value_0[1] = {168, 169};
        phv_id_to_save_hash_value_0[2] = {170, 171};

        auto& salu_id_0 = salu_id[0];
        salu_id_0 = {224, 225, 226};
        // processor1 finished

        ProcessorConfig proc1 = ProcessorConfig(1);

        auto& salu_id_1 = salu_id[1];
        salu_id_1 = {224, 225, 226, 227};
        // if phv_162 >= phv_163, phv_165 = phv_162; else: phv_165 = phv_163
        auto& salu3 = SALUs[1][0];
        salu3.op = SALUnit::OP::IfElseRAW;
        salu3.left_value.type = SALUnit::Parameter::Type::HEADER;
        salu3.left_value.content.phv_id = 162;
        salu3.left_value.if_type = SALUnit::Parameter::IfType::GTE;
        salu3.operand1.type = SALUnit::Parameter::Type::HEADER;
        salu3.operand1.content.phv_id = 163;
        salu3.operand2.type = SALUnit::Parameter::Type::CONST;
        salu3.operand2.content.value = 0;
        salu3.operand3.type = SALUnit::Parameter::Type::CONST;
        salu3.operand3.content.value = 0;
        salu3.return_value.type = SALUnit::Parameter::Type::HEADER;
        salu3.return_value.content.phv_id = 165;
        salu3.return_value_from.type = SALUnit::ReturnValueFrom::Type::LEFT;
        salu3.return_value_from.false_type = SALUnit::ReturnValueFrom::Type::OP1;

        // if phv_162 >= phv_163, phv_1 = 0; else: phv_1 = 1;
        auto& salu4 = SALUs[1][1];
        salu4.op = SALUnit::OP::IfElseRAW;
        salu4.left_value.type = SALUnit::Parameter::Type::HEADER;
        salu4.left_value.content.phv_id = 162;
        salu4.left_value.if_type = SALUnit::Parameter::IfType::GTE;
        salu4.operand1.type = SALUnit::Parameter::Type::HEADER;
        salu4.operand1.content.phv_id = 163;
        salu4.operand2.type = SALUnit::Parameter::Type::CONST;
        salu4.operand2.content.value = 0;
        salu4.operand3.type = SALUnit::Parameter::Type::CONST;
        salu4.operand3.content.value = 0;
        salu4.return_value.type = SALUnit::Parameter::Type::HEADER;
        salu4.return_value.content.phv_id = 1;
        salu4.return_value_from.type = SALUnit::ReturnValueFrom::CONST;
        salu4.return_value_from.content.value = 0;
        salu4.return_value_from.false_type = SALUnit::ReturnValueFrom::CONST;
        salu4.return_value_from.false_content.value = 1;
        // processor_2 finished, using salu instead of normal alu

        ProcessorConfig proc2 = ProcessorConfig(2);
        proc2.push_back_get_key_use_container(164, 32, 0, 1, 2, 3);
        proc2.push_back_get_key_use_container(165, 32, 4, 5, 6, 7);
        proc2.push_back_get_key_use_container(1, 8, 8);

        auto& gate1 = proc2.gatewaysConfig.gates[0];
        gate1.op = GatewaysConfig::Gate::LT;
        gate1.operand1.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate1.operand1.content.operand_match_field_byte = {4, {0, 1, 2, 3}};
        gate1.operand2.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate1.operand2.content.operand_match_field_byte = {4, {4, 5, 6, 7}};

        auto& gate2 = proc2.gatewaysConfig.gates[1];
        gate2.op = GatewaysConfig::Gate::NEQ;
        gate2.operand1.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate2.operand1.content.operand_match_field_byte = {4, {0, 1, 2, 3}};
        gate2.operand2.type = GatewaysConfig::Gate::Parameter::CONST;
        gate2.operand2.content.value = 0;

        auto& gate3 = proc2.gatewaysConfig.gates[2];
        gate3.op = GatewaysConfig::Gate::GTE;
        gate3.operand1.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate3.operand1.content.operand_match_field_byte = {4, {0, 1, 2, 3}};
        gate3.operand2.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate3.operand2.content.operand_match_field_byte = {4, {4, 5, 6, 7}};

        auto& gate4 = proc2.gatewaysConfig.gates[3];
        gate4.op = GatewaysConfig::Gate::NEQ;
        gate4.operand1.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate4.operand1.content.operand_match_field_byte = {4, {4, 5, 6, 7}};
        gate4.operand2.type = GatewaysConfig::Gate::Parameter::CONST;
        gate4.operand2.content.value = 0;

        auto& gate5 = proc2.gatewaysConfig.gates[4];
        gate5.op = GatewaysConfig::Gate::EQ;
        gate5.operand1.type = GatewaysConfig::Gate::Parameter::HEADER;
        gate5.operand1.content.operand_match_field_byte = {1, {8}};
        gate5.operand2.type = GatewaysConfig::Gate::Parameter::CONST;
        gate5.operand2.content.value = 0;

        key = {false};
        value = {false};
        key[0] = key[1] = true;
        value[32] = true;
        proc2.insert_gateway_res_2_match_table(key, value);
        key = {false};
        key[1] = key[2] = key[3] = key[4] = true;
        value = {false};
        value[33] = true;
        proc2.insert_gateway_res_2_match_table(key, value);
        // todo: add other entries

        gatewaysConfigs[2].masks.push_back(std::array<bool, 16>{true, true, false});

        matchTableConfigs[2].matchTables[0].default_action_id = 5;
        matchTableConfigs[2].matchTables[1].default_action_id = 6;
        matchTableConfigs[2].matchTables[2].default_action_id = 7;

        auto& action = actionConfigs[2].actions[5];
        action.action_id = 5;
        action.vliw_enabler = {false};
        action.vliw_enabler[163] = true;
        action.alu_configs[163].op = ALUnit::SET;
        action.alu_configs[163].operand1.type = ALUnit::Parameter::CONST;
        action.alu_configs[163].operand1.content.value = 1;
        action.vliw_enabler[164] = true;
        action.alu_configs[164].op = ALUnit::SET;
        action.alu_configs[164].operand1.type = ALUnit::Parameter::CONST;
        action.alu_configs[164].operand1.content.value = 1;

        auto& action1 = actionConfigs[2].actions[6];
        action1.action_id = 6;
        action1.vliw_enabler = {false};
        action1.vliw_enabler[162] = true;
        action1.alu_configs[162].op = ALUnit::SET;
        action1.alu_configs[162].operand1.type = ALUnit::Parameter::CONST;
        action1.alu_configs[162].operand1.content.value = 1;

        auto& action2 = actionConfigs[2].actions[7];
        action2.action_id = 7;
        action2.vliw_enabler = {false};
        action2.vliw_enabler[163] = true;
        action2.alu_configs[163].op = ALUnit::SET;
        action2.alu_configs[163].operand1.type = ALUnit::Parameter::CONST;
        action2.alu_configs[163].operand1.content.value = 1;

        proc_types[2] = ProcType::WRITE;
        // processor_3 finished
    }
};

#endif // RPISA_SW_SWITCH_H
