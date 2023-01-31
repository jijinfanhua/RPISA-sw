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

#include <memory>
const int PACKET_SIZE_MAX = 1600;
using byte = unsigned char;


struct ProcessorConfig {

    GetKeyConfig     getKeyConfig;
    GatewaysConfig   gatewaysConfig;
    MatchTableConfig matchTableConfig;
    ActionConfig     actionConfig;

    int processor_id;

    ProcessorConfig(int id): processor_id(id) {
        getKeyConfig.processor_id = id;
        getKeyConfig.used_container_num = 0;
        gatewaysConfig.processor_id = id;
        matchTableConfig.processor_id = id;
        actionConfig.processor_id = id;
    }

    void push_back_get_key_use_container(
            int id, int size,
            int match_field1,
            int match_field2 = -1,
            int match_field3 = -1,
            int match_field4 = -1) {
        int k = getKeyConfig.used_container_num;
        GetKeyConfig::UsedContainer2MatchFieldByte x;
        x.match_field_byte_ids[0] = match_field1;
        x.match_field_byte_ids[1] = match_field2;
        x.match_field_byte_ids[2] = match_field3;
        x.match_field_byte_ids[3] = match_field4;
        x.used_container_id = id;

        switch (size) {
            case 8: {
                x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U8;
                for (int i = 0; i < 1; i++) {
                    if (x.match_field_byte_ids[i] == -1) {
                        throw "match field " + to_string(k) + "id error or insufficient";
                    }
                }
                break;
            }
            case 16: {
                x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U16;
                for (int i = 0; i < 2; i++) {
                    if (x.match_field_byte_ids[i] == -1) {
                        throw "match field " + to_string(k) + "id error or insufficient";
                    }
                }
                break;
            }
            case 32: {
                x.container_type = GetKeyConfig::UsedContainer2MatchFieldByte::U32;
                for (int i = 0; i < 4; i++) {
                    if (x.match_field_byte_ids[i] == -1) {
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
    static u32 bool_array_2_u32(const array<bool, GATEWAY_NUM> bool_array) {
        u32 sum = 0;
        for(int i = 0 ; i < 16; i++) {
            sum += (1 << (15 - i));
        }
        return sum;
    }
public:

    void insert_gateway_res_2_match_table(
            const array<bool, GATEWAY_NUM>& key,
            const array<bool, MAX_PARALLEL_MATCH_NUM*PROCESSOR_NUM>& value) {
        gatewaysConfig.gateway_res_2_match_tables[bool_array_2_u32(key)] = value;
    }

    void insert_gateway_res_2_gates(
            const array<bool, GATEWAY_NUM>& key,
            const array<bool, MAX_GATEWAY_NUM*PROCESSOR_NUM>& value
            ) {
        gatewaysConfig.gateway_res_2_gates[bool_array_2_u32(key)]= value;
    }

    GatewaysConfig::Gate::Parameter param(int value) {
        GatewaysConfig::Gate::Parameter parameter;
        parameter.type = GatewaysConfig::Gate::Parameter::Type::CONST;
        parameter.content.value = value;
        return parameter;
    }

    GatewaysConfig::Gate::Parameter param(int len, int id1, int id2 = -1, int id3 = -1, int id4 = -1) {
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
            GatewaysConfig::Gate::Parameter operand2) {
        gatewaysConfig.gates[id].op = op;
        gatewaysConfig.gates[id].operand1 = operand1;
        gatewaysConfig.gates[id].operand2 = operand2;

    }

/*
 * struct MatchTableConfig {
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
 */


    void set_action_param_num(int id, int action_id, int action_data_num, const array<bool, MAX_PHV_CONTAINER_NUM>& vliw_enabler) {
        auto& action = actionConfig.actions[id];
        action.action_id = action_id;
        action.action_data_num = action_data_num;
        action.vliw_enabler = vliw_enabler;
    }


    void commit() const {
        getKeyConfigs    [processor_id] = getKeyConfig;
        gatewaysConfigs  [processor_id] = gatewaysConfig;
        matchTableConfigs[processor_id] = matchTableConfig;
        actionConfigs    [processor_id] = actionConfig;
    }




};

void commit(const vector<ProcessorConfig>& config) {
    for (int i = 0; i < config.size(); i++) {
        config[i].commit();
    }
}

struct Switch {

    Parser parser;
    PipeLine pipeline;
    vector<unique_ptr<Logic>> logics;
    int proc_num_config;

    Switch() {
        for (int i = 0; i < proc_num_config; i++) {
            // todo: Logic 需要一个虚析构函数，但现在不算什么大问题还
            logics.push_back(make_unique<GetKey>       (i));
            logics.push_back(make_unique<Gateway>      (i));
            logics.push_back(make_unique<GetHash>      (i));
            logics.push_back(make_unique<GetAddress>   (i));
            logics.push_back(make_unique<Matches>      (i));
            logics.push_back(make_unique<Compare>      (i));
            logics.push_back(make_unique<GetAction>    (i));
            logics.push_back(make_unique<ExecuteAction>(i));
        }
    }

    void GetInput(int interface, const Packet& packet, PipeLine& pipeline) {
        if (interface != 0) {
            PHV phv = parser.parse(packet);
            pipeline.processors[0].getKeys.enable1 = true;
            pipeline.processors[0].getKeys.phv = phv;
            pipeline.processors[0].getKeys.gateway_guider = {};
            pipeline.processors[0].getKeys.match_table_guider = {};
        } else {
            pipeline.processors[0].getKeys.enable1 = false;
            pipeline.processors[0].getKeys.phv = {};
            pipeline.processors[0].getKeys.gateway_guider = {};
            pipeline.processors[0].getKeys.match_table_guider = {};
        }

    }

    void Execute(int interface, const Packet& packet) {
        PipeLine next;
        GetInput(interface, packet, next);
        for (auto& logic : logics) {
            logic->execute(pipeline, next);
        }
        GetOutput();

        pipeline = next;

    }


    void GetOutput() {
        BaseRegister& output = pipeline.processors[proc_num_config].base;
        if (output.enable1) {
            // todo: 这里实际是找到payload，然后给phv装上去，现在还没有材料可以用
            output.phv;
        }
    }

    void Config()




};


#endif //RPISA_SW_SWITCH_H
