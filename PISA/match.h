//
// Created by root on 1/23/23.
//

#ifndef RPISA_SW_MATCH_H
#define RPISA_SW_MATCH_H

#include "../pipeline.h"
#include "../dataplane_config.h"

struct ArrayHash {
    size_t operator()(const std::array<u32, 128>& arr) const {
        std::hash<u32> hasher;
        size_t hash_val = 0;
        for (int i = 0; i < 128; i++) {
            hash_val ^= hasher(arr[i]) + 0x9e3779b9 + (hash_val<<6) + (hash_val>>2);
        }
        return hash_val & ((1ull << 52) - 1);
    }
};

u64 get_flow_id(PHV phv)
{
    // done: fetch flow id from two particular positions of phv
    auto result = u32_to_u64(phv[flow_id_in_phv[0]], phv[flow_id_in_phv[1]]);
    return result;
}

struct GetKey : public Logic
{
    GetKey(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const GetKeysRegister &getKey = now->processors[processor_id].getKeys;
        HashRegister &nextHash = next->processors[processor_id].hashes;

        getKeyFromPHV(getKey, nextHash);
    }

    void getKeyFromPHV(const GetKeysRegister &now, HashRegister &next)
    {
        next.enable1 = now.enable1;
        if (!now.enable1)
        {
            return;
        }

        GetKeyConfig getKeyConfig = getKeyConfigs[processor_id];
        for (int i = 0; i < getKeyConfig.used_container_num; i++)
        {
            GetKeyConfig::UsedContainer2MatchFieldByte it = getKeyConfig.used_container_2_match_field_byte[i];
            int id = it.used_container_id;
            if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U8)
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id];
            }
            else if (it.container_type == GetKeyConfig::UsedContainer2MatchFieldByte::U16)
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id] << 16 >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[id] << 24 >> 24;
            }
            else
            {
                next.key[it.match_field_byte_ids[0]] = now.phv[id] >> 24;
                next.key[it.match_field_byte_ids[1]] = now.phv[id] << 8 >> 24;
                next.key[it.match_field_byte_ids[2]] = now.phv[id] << 16 >> 24;
                next.key[it.match_field_byte_ids[3]] = now.phv[id] << 24 >> 24;
            }
        }

        next.phv = now.phv;
        next.gateway_guider = now.gateway_guider;
        next.match_table_guider = now.match_table_guider;
    }
};

struct GetHash : public Logic
{
    GetHash(int id) : Logic(id) {}

    void execute(const PipeLine *now, PipeLine *next) override
    {
        const HashRegister &hashReg = now->processors[processor_id].hashes;
        DispatcherRegister &dpRegister = next->processors[processor_id].dp;

        auto phv = hashReg.phv;
        ArrayHash hasher;
        u64 flow_id = hasher(hashReg.key);
        // high 32 bit
        phv[flow_id_in_phv[0]] = flow_id >> 32;
        // low 32 bit
        phv[flow_id_in_phv[1]] = flow_id << 32 >> 32;

        dpRegister.enable1 = hashReg.enable1;

        dpRegister.dq_item.phv = phv;
    }
};

struct Dispatcher: public Logic{
    Dispatcher(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{
        const DispatcherRegister& now_dp_0 = now->processors[processor_id].dp;
        OperateRegister &operate = next->processors[processor_id].op[0];

        dispatcher_cycle(now_dp_0, operate, now->proc_states[processor_id], next->proc_states[processor_id], now);
    }

    bool test_flow_occupied(DispatcherQueueItem dp_item, const PipeLine* now){
        for(auto reg: now->processors[processor_id].op){
            if(get_flow_id(reg.phv) == get_flow_id(dp_item.phv)){
                return true;
            }
        }
        return false;
    };

    void dispatcher_cycle(const DispatcherRegister& now, OperateRegister& next, const ProcessorState& now_proc, ProcessorState& next_proc, const PipeLine* now_pipe){
        if(now.enable1){
            // if pipeline has packet
            u32 queue_id = get_flow_id(now.dq_item.phv) & (dispatcher_queue_width-1);
            if(now_proc.dispatcher_queues[queue_id].size() == max_queue_size_allowed){
                // queue reached max, packet will be lost
                next_proc.packet_lost += 1;
            }
            else{
                next_proc.dispatcher_queues[queue_id].push_back(now.dq_item);
            }
        }
        // get schedule id
        int schedule_id = now_proc.schedule_id;
        int queues_count = 0;
        while(now_proc.dispatcher_queues[schedule_id].empty() || test_flow_occupied(now_proc.dispatcher_queues[schedule_id].front(), now_pipe)){
            // if queue empty or first flow occupied
            schedule_id = (schedule_id + 1) % dispatcher_queue_width;
            queues_count += 1;
            if(queues_count == dispatcher_queue_width) break;
        }
        next_proc.schedule_id = schedule_id;

        if(queues_count == dispatcher_queue_width ){
            next.enable1 = false;
            return;
        }
        else {
            next.enable1 = true;
            auto dp_item = now_proc.dispatcher_queues[schedule_id].front();
            next.phv = dp_item.phv;
            next_proc.dispatcher_queues[schedule_id].erase(next_proc.dispatcher_queues[schedule_id].begin());
        }
    }
};

struct Operate: public Logic{
    Operate(int id): Logic(id){}

    void execute(const PipeLine* now, PipeLine* next) override{
        for(int i = 1; i < processing_function_num; i++){
            next->processors[processor_id].op[i].phv = now->processors[processor_id].op[i-1].phv;
            next->processors[processor_id].op[i].enable1 = now->processors[processor_id].op[i-1].enable1;
        }
    }
};

#endif // RPISA_SW_MATCH_H
