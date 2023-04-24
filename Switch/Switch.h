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

    int processor_id;

    explicit ProcessorConfig(int id) : processor_id(id)
    {
        getKeyConfig.used_container_num = 0;
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

    void commit() const
    {
        getKeyConfigs[processor_id] = getKeyConfig;
    }
};


void flowblaze_config(){
    flow_id_in_phv = {170, 171};

    ProcessorConfig proc0 = ProcessorConfig(0);
    proc0.push_back_get_key_use_container(0, 8, 0);
    proc0.push_back_get_key_use_container(64, 16, 1, 2);
    proc0.push_back_get_key_use_container(65, 16, 3, 4);
    proc0.push_back_get_key_use_container(160, 32, 5,6,7,8);
    proc0.push_back_get_key_use_container(161, 32, 9, 10, 11, 12);

    proc0.commit();
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
        for (int i = 0; i < PROC_NUM; i++)
        {
            // done Logic 需要一个虚析构函数，但现在不算什么大问题还
            logics.push_back(make_unique<GetKey>(i));
            logics.push_back(make_unique<GetHash>(i));
            logics.push_back(make_unique<Dispatcher>(i));
            logics.push_back(make_unique<Operate>(i));
        }
    }

    void GetInput(int interface, const Packet &packet, PipeLine* pipeline_, int arrive_id)
    {
        if (interface != 0)
        {
            PHV phv = parser.parse(packet);
            phv[ID_IN_PHV] = arrive_id;
            pipeline_->processors[0].getKeys.enable1 = true;
            pipeline_->processors[0].getKeys.phv = phv;
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
        GetInput(interface, packet, next, arrive_id);
        for(int i = 0; i < PROC_NUM; i++){
            update();
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
    }

    std::pair<u64, int> get_output_arrive_id() const{
        OperateRegister &output = pipeline->processors[PROC_NUM-1].op[processing_function_num-1];
        if(output.enable1){
            u64 flow_id = u32_to_u64(output.phv[flow_id_in_phv[0]], output.phv[flow_id_in_phv[1]]);
            return {flow_id, output.phv[ID_IN_PHV]};
        }
        else{
            return {-1, -1};
        }
    }

    void Log(std::array<ofstream*, PROC_NUM> outputs, std::array<bool, PROC_NUM> enable, int total_cycles){
        for(int i = 0; i < PROC_NUM; i++){
            if(enable[i]){
                pipeline->proc_states[i].log(*outputs[i], total_cycles);
            }
        }
    };

    void update(){
        for(int i = 0; i < PROC_NUM; i++){
                pipeline->proc_states[i].update();
        }
    }

    void Config()
    {
        flowblaze_config();
    }
};

#endif // RPISA_SW_SWITCH_H
