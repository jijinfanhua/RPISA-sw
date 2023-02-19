//
// Created by Walker on 2023/1/29.
//

#ifndef RPISA_SW_SWITCH_H
#define RPISA_SW_SWITCH_H

#include "Parser.h"
#include "../PipeLine.h"
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

    GetKeyConfig getKeyConfig;
    GatewaysConfig gatewaysConfig;
    MatchTableConfig matchTableConfig;
    ActionConfig actionConfig;

    int processor_id;

    ProcessorConfig(int id) : processor_id(id)
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

    void set_action_param_num(int id, int action_id, int action_data_num, const array<bool, MAX_PHV_CONTAINER_NUM + MAX_SALU_NUM> &vliw_enabler)
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
    PipeLine pipeline;
    vector<unique_ptr<Logic>> logics;
    int proc_num_config;

    Switch()
    {
        for (int i = 0; i < proc_num_config; i++)
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

    void GetInput(int interface, const Packet &packet, PipeLine &pipeline)
    {
        if (interface != 0)
        {
            PHV phv = parser.parse(packet);
            pipeline.processors[0].getKeys.enable1 = true;
            pipeline.processors[0].getKeys.phv = phv;
            pipeline.processors[0].getKeys.gateway_guider = {};
            pipeline.processors[0].getKeys.match_table_guider = {};
        }
        else
        {
            pipeline.processors[0].getKeys.enable1 = false;
            pipeline.processors[0].getKeys.phv = {};
            pipeline.processors[0].getKeys.gateway_guider = {};
            pipeline.processors[0].getKeys.match_table_guider = {};
        }
    }

    void Execute(int interface, const Packet &packet)
    {
        PipeLine next;
        GetInput(interface, packet, next);
        for (auto &logic : logics)
        {
            logic->execute(pipeline, next);
        }
        GetOutput();

        pipeline = next;
    }

    void GetOutput()
    {
        BaseRegister &output = pipeline.processors[proc_num_config].base;
        if (output.enable1)
        {
            // todo: 这里实际是找到payload，然后给phv装上去，现在还没有材料可以用
            output.phv;
        }
    }

    void log(){
        for(int i = 0; i < PROC_NUM; i++){
            pipeline.proc_states[i].log();
        }
    };

    void Config()
    {
        ProcessorConfig proc0 = ProcessorConfig(0);
        proc0.push_back_get_key_use_container(0, 8, 0);
        proc0.push_back_get_key_use_container(64, 16, 1, 2);
        proc0.push_back_get_key_use_container(65, 16, 3, 4);
        proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
        proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

        array<bool, GATEWAY_NUM> key = {false};
        array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> value = {0};
        value[0] = value[1] = value[2] = 1;

        proc0.matchTableConfig.match_table_num = 3;
        auto& config = proc0.matchTableConfig.matchTables[0];
        config.type = 1;
        config.depth = 1;
        config.key_width = 1;
        config.value_width = 1;
        config.hash_in_phv = 166;
        config.match_field_byte_len = 13;
        config.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config.number_of_hash_ways = 1;
        config.hash_bit_sum = 12;
        config.hash_bit_per_way = {12};
        config.srams_per_hash_way = {8};
        config.key_sram_index_per_hash_way = {0,1,2,3};
        config.value_sram_index_per_hash_way = {4,5,6,7};

        auto& config = proc0.matchTableConfig.matchTables[1];
        config.type = 1;
        config.depth = 4;
        config.key_width = 1;
        config.value_width = 1;
        config.hash_in_phv = 167;
        config.match_field_byte_len = 13;
        config.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config.number_of_hash_ways = 1;
        config.hash_bit_sum = 12;
        config.hash_bit_per_way = {12};
        config.srams_per_hash_way = {8};
        config.key_sram_index_per_hash_way = {8,9,10,11};
        config.value_sram_index_per_hash_way = {12,13,14,15};

        config.type = 1;
        config.depth = 4;
        config.key_width = 1;
        config.value_width = 1;
        config.hash_in_phv = 166;
        config.match_field_byte_len = 13;
        config.match_field_byte_ids = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        config.number_of_hash_ways = 1;
        config.hash_bit_sum = 12;
        config.hash_bit_per_way = {12};
        config.srams_per_hash_way = {8};
        config.key_sram_index_per_hash_way = {16,17,18,19};
        config.value_sram_index_per_hash_way = {20,21,22,23};

        array<bool, 240> vliw_enabler = {0};
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
        salu.salu_id = 0;
        salu.op = SALUnit::RAW;
        
    }
};

#endif // RPISA_SW_SWITCH_H
