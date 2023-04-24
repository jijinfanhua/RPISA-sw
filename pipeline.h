//
// Created by root on 1/28/23.
//

#ifndef RPISA_SW_PIPELINE_H
#define RPISA_SW_PIPELINE_H

#include "PISA/Register/Register.h"
#include "RPISA/Register/RRegister.h"
#include "defs.h"
// 这里记录了所有的有关于时序的信息，包括buffer的状态，各种流水的走法等
// 每一个Register应自己管理好自己所经营范围内的phv流水线

struct FlowInfo {
    PHV phv;
    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values; // for stateless table
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys; // for stateless table
    bool backward_pkt;
};
struct WriteInfo {
    std::array<u64, 4> write_addr;
    u64 flow_addr;
    std::array<u32, 128> state;
};
struct CDInfo { // cancel_dirty info
    u64 addr;
};
struct RP2R_REG {
    RingRegister rr;
    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
};

struct ProcessorState {
    // flowblaze
    u32 schedule_id = 0;
    std::array<std::vector<DispatcherQueueItem>, 16> dispatcher_queues;

    // statistics

    std::array<int, 16>  m_dispatcher_queue;
    std::array<int, 16> total_dispatcher_queue;

    int packet_lost = 0;

    ProcessorState& operator=(const ProcessorState &other) = default;

    void update(){
        for(int i = 0; i < dispatcher_queue_width; i++){
            total_dispatcher_queue[i] += dispatcher_queues[i].size();
            if(dispatcher_queues[i].size() > m_dispatcher_queue[i]){
                m_dispatcher_queue[i] = dispatcher_queues[i].size();
            }
        }
    }

    void log(ofstream& output, int total_cycles) {
        for(int i = 0; i < dispatcher_queue_width; i++){
            output << i << "th queue max size: " << m_dispatcher_queue[i] << endl;
        }
        output << "=====" << endl;
        for(int i = 0; i < dispatcher_queue_width; i++){
            output << i << "th queue average size: " << total_dispatcher_queue[i] / total_cycles << endl;
        }
    }
};

struct ProcessorRegister {
    GetKeysRegister getKeys{};
    HashRegister hashes{};
    DispatcherRegister dp{};
    OperateRegister op[250]{};
};


struct PipeLine {

    // And god said, let here be processor, and here was processor.
    std::array<ProcessorRegister, PROC_NUM> processors;
    std::array<ProcessorState, PROC_NUM> proc_states;

};

struct Logic {

    // 每一个Logic都应标识自己所在的processor id
    int processor_id;

    explicit Logic(int processor_id) : processor_id(processor_id) {}

    // 每一个Logic都应实现这个时序算法，在pipeline中找到自己应该操控的寄存器，
    // now表示当前时钟的状态，通过对next中的寄存器赋值，来实现对下一个时钟的操作
    // 说起来简单，但务必注意，所有的时序寄存器都在PipeLine的Register结构体中，与Logic是割离的
    // Logic中不应存有任何与时序有关的Register的值，而只负责组合行为和必要配置
    virtual void execute(const PipeLine* now, PipeLine* next) = 0;

    virtual ~Logic()= default;
};

#endif //RPISA_SW_PIPELINE_H
