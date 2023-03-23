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
    u32 decrease_clk{}, increase_clk{};
//    bool clk_enable = false;
    // hash of four ways (32bit) -> flow info
    std::unordered_map<u64, flow_info_in_cam> dirty_cam;

    // 128 will cause segmentation fault, reason unknown
//    std::array<FlowInfo, 128> rp2p{};
//    // 指示下一个对象在rp2p中的位置
//    std::array<u32, 128> rp2p_pointer{};
    std::unordered_map<u64, std::vector<FlowInfo>> r2p;
    std::unordered_map<u64, std::vector<FlowInfo>> p2p;

    std::queue<FlowInfo> r2p_stash;
    std::queue<WriteInfo> write_stash;

    std::vector<flow_info_in_cam> wait_queue;

    std::vector<flow_info_in_cam> schedule_queue;

    queue<RP2R_REG> p2r;
    queue<RP2R_REG> r2r;
    int round_robin_flag = 0; // 0 is r2r, 1 is p2r
    int hb_robin_flag = 0;

    // flowblaze
    u32 schedule_id = 0;
    std::array<std::vector<DispatcherQueueItem>, 16> dispatcher_queues;

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

    ProcessorState& operator=(const ProcessorState &other) = default;

    void update(){
        if(wait_queue.size() > m_wait_queue){
            m_wait_queue = wait_queue.size();
        }
        if(schedule_queue.size() > m_schedule_queue){
            m_schedule_queue = schedule_queue.size();
        }
        if(dirty_cam.size() > max_dirty_cam){
            max_dirty_cam = dirty_cam.size();
        }
        if(r2p.size() + p2p.size() > rp2p_max){
            rp2p_max = r2p.size() + p2p.size();
        }
        if(r2p_stash.size() > r2p_stash_max){
            r2p_stash_max = r2p_stash.size();
        }
        if(write_stash.size() > write_stash_max){
            r2p_stash_max = write_stash.size();
        }
        if(p2r.size() > p2r_max){
            p2r_max = p2r.size();
        }
        if(r2r.size() > r2r_max ){
            r2r_max = r2r.size();
        }
    }

    void log(ofstream& output) {
        output << "max wait queue: " << m_wait_queue << endl;
        output << "r2p stash max: " << r2p_stash_max << endl;
        output << "write stash max: " << write_stash_max << endl;
        output << "p2r max: " << p2r_max << endl;
        output << "r2r max: " << r2r_max << endl;
        output << "rp2p max: " << rp2p_max << endl;
        output << "max dirty cam: " << max_dirty_cam << endl;
        output << "max schedule queue: " << m_schedule_queue << endl;
        output << "wb_packets: " << wb_packets << endl;
        output << "bp_packets: " << bp_packets << endl;
        output << "total_packets: " << total_packets << endl;
        output << "hb_cycles: " << hb_cycles << endl;
        output << "cc_send: " << cc_send << endl;
        output << "cc_received: " << cc_received << endl;
        output << "ro_empty: " << ro_empty << endl;
        output << "r2r_schedule: " << r2r_schedule << endl;
        output << dirty_cam.size() << endl;
        output << wait_queue.size() << endl;
        output << schedule_queue.size() << endl;
        output << r2p_stash.size() << endl;
        output << p2r.size() << endl;
        output << r2r.size() << endl;
    }
};

struct ProcessorRegister {

    /****** fengyong add begin ********/
    BaseRegister base{};
    GetKeysRegister getKeys{};
    GatewayRegister gateway[2]{};
    HashRegister hashes[4]{};
    DispatcherRegister dp{};
    GetAddressRegister getAddress{};
    MatchRegister match{};
    CompareRegister compare{};
    ConditionEvaluationRegister conditionEvaluation{};
    GetActionRegister getAction;
    ExecuteActionRegister executeAction{};
    KeyRefactorRegister refactor{};
    EfsmHashRegister efsmHash{};
    /****** fengyong add end *********/
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
