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
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    bool backward_pkt;
    std::array<int, 4> table_id;
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
    bool normal_pipe_pkt{};

    // 128 will cause segmentation fault, reason unknown
    std::array<FlowInfo, 32> rp2p{};
    // 指示下一个对象在rp2p中的位置
    std::array<u32, 128> rp2p_pointer{};
    int rp2p_tail = 0;
    std::queue<FlowInfo> r2p_stash;
    std::queue<WriteInfo> write_stash;

    queue<flow_info_in_cam> wait_queue;
    u32 wait_head{}, wait_tail{};
    flow_info_in_cam wait_queue_head;
    bool wait_queue_head_flag{};
    u32 write_proc_id{};
    u32 read_proc_id{};

    bool normal_pipe_schedule_flag{};
    queue<flow_info_in_cam> schedule_queue;

    queue<RP2R_REG> p2r;
    queue<RP2R_REG> r2r;
    int round_robin_flag = 0;// 0 is r2r, 1 is p2r

    ProcessorState& operator=(const ProcessorState &other) = default;

    void log() {
        cout << "normal_pipe_schedule_flag: " << normal_pipe_schedule_flag << endl;
    }
};

struct ProcessorRegister {

    /****** fengyong add begin ********/
    BaseRegister base{};
    GetKeysRegister getKeys{};
    GatewayRegister gateway[2]{};
    HashRegister hashes[4]{};
    GetAddressRegister getAddress{};
    MatchRegister match{};
    CompareRegister compare{};
    GetActionRegister getAction;
    ExecuteActionRegister executeAction{};
    VerifyStateChangeRegister verifyState;
    /****** fengyong add end *********/

    PIRegister pi[2];
    PIRAsynRegister pi_asyn[2]{};
    PORegister po{};
    RIRegister ri{};
    RORegister ro{};
    PIWRegister piw[2];
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
