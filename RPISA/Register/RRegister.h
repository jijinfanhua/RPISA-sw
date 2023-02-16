//
// Created by root on 1/28/23.
//

#ifndef RPISA_SW_RREGISTER_H
#define RPISA_SW_RREGISTER_H

#include "../../defs.h"
#include "../../dataplane_config.h"

/*********** fengyong add start ***************/
enum CAM_SEARCH_RES {
    PIPE_FOUND,
    PIPE_NOT_FOUND,
    BP_FOUND,
    BP_NOT_FOUND,
    WRITE_FOUND,
    WRITE_NOT_FOUND
};

struct RingRegister {
    u32 dst1;
    u32 ctrl;
    u32 dst2;
    u32 addr;
    std::array<u32, 128> payload;
};

struct flow_info_in_cam {
    b128 hash_value;
    u32 timer_offset;
    int r2p_first_pkt_idx;
    int r2p_last_pkt_idx;
    int p2p_first_pkt_idx;
    int p2p_last_pkt_idx;
    u32 flow_addr;
    bool lazy = false;

    u32 left_pkt_num;
    enum FSMState {
        RUN, WAIT, SUSPEND, READY
    } cur_state;

    flow_info_in_cam& operator=(const flow_info_in_cam &other) = default;

};

struct RingBaseRegister {
    bool enable1;
    PHV phv;

    std::array<bool, MAX_PARALLEL_MATCH_NUM * PROCESSOR_NUM> match_table_guider;
    std::array<bool, MAX_GATEWAY_NUM * PROCESSOR_NUM> gateway_guider;
};

struct PIRegister : public RingBaseRegister{
    bool need_to_block;
    RingRegister ringReg;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    flow_info_in_cam flow_info;
};

struct RIRegister : public RingBaseRegister {
    RingRegister ringReg;
    b128 hash_value;
};

struct PORegister : public RingBaseRegister {
    RingRegister ringReg;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    std::array<std::array<u32, 32>, MAX_PARALLEL_MATCH_NUM> match_table_keys;
    bool normal_pipe_schedule;
    b128 hash_value;
};

struct RORegister : public RingBaseRegister {
    RingRegister ringReg;
//    bool normal_pipe_schedule;
};

struct PIRAsynRegister : public RingBaseRegister {
    RingRegister ringReg;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    b128 hash_value;
//    bool cam_hit;
    CAM_SEARCH_RES cam_search_res;
};

struct PIWRegister : public RingBaseRegister {
//    RingRegister ringReg;
//    bool normal_pipe_schedule;
    std::array<std::array<u32, 4>, MAX_PARALLEL_MATCH_NUM> hash_values;
    b128 hash_value;

    u64 cd_addr;
    bool cd_come = false;

    flow_info_in_cam flow_info;
    bool state_changed; // need to write state
//    bool wb_flag; // need write state
    bool state_writeback;
    bool pkt_backward;
};

/*********** fengyong add end ***************/

#endif //RPISA_SW_RREGISTER_H
