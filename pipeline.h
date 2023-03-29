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
    std::array<std::vector<DispatcherQueueItem>, dispatcher_queue_width> dispatcher_queues;

    // statistics
    int m_wait_queue = 0;
    int m_schedule_queue = 0;
    int total_packets = 0;
    int wb_packets = 0;
    int bp_packets = 0;
    int hb_cycles = 0;
    int po_normal = 0;
    int piw_packet = 0;
    int cc_send = 0;
    int cc_received = 0;
    int ro_empty = 0;
    int p2r_schedule = 0;
    int r2r_schedule = 0;
    int max_dirty_cam = 0;
    int rp2p_max = 0;
    int r2p_stash_max = 0;
    int write_stash_max = 0;
    int p2r_max = 0;
    int r2r_max = 0;

    std::array<int, dispatcher_queue_width>  m_dispatcher_queue;

    ProcessorState& operator=(const ProcessorState &other) = default;

    void update(){
        for(int i = 0; i < dispatcher_queue_width; i++){
            if(dispatcher_queues[i].size() > m_dispatcher_queue[i]){
                m_dispatcher_queue[i] = dispatcher_queues[i].size();
            }
        }
    }

    void log(ofstream& output) {
        for(int i = 0; i < dispatcher_queue_width; i++){
            output << i << "th queue max size: " << m_dispatcher_queue[i] << endl;
        }
    }
};

struct ProcessorRegister {
    GetKeysRegister getKeys{};
    GatewayRegister gateway[2]{};
    HashRegister hashes[4]{};
    DispatcherRegister dp{};
    GetAddressRegister getAddress{};
    MatchRegister match{};
    CompareRegister compare{};
    ConditionEvaluationRegister conditionEvaluation{};
    KeyRefactorRegister refactor{};
    EfsmHashRegister efsmHash[4]{};
    EfsmGetAddressRegister efsmGetAddress{};
    EfsmMatchRegister efsmMatch{};
    EfsmCompareRegister efsmCompare{};
    GetActionRegister getAction;
    ExecuteActionRegister executeAction{};
    UpdateStateRegister update{};
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
